#include "update_manager.h"
#include "progress_dialog.h"
#include "localized.h"
#include "rpcs3_version.h"
#include "downloader.h"
#include "gui_settings.h"
#include "Utilities/File.h"
#include "Emu/System.h"
#include "Crypto/utils.h"
#include "util/logs.hpp"
#include "util/types.hpp"
#include "util/sysinfo.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QDateTime>
#include <QMessageBox>
#include <QLabel>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QThread>
#include <QProcess>

#ifndef _WIN32
#include <unistd.h>
#include <sys/stat.h>
#endif

LOG_CHANNEL(update_log, "UPDATER");

namespace
{
	constexpr auto GITHUB_API_URL = "https://api.github.com/repos/dylan-buck/RPCS3-Mac-Build/releases/latest";
	constexpr auto GITHUB_DOWNLOAD_PREFIX = "https://github.com/dylan-buck/RPCS3-Mac-Build/";
	constexpr auto ZIP_ASSET_SUFFIX = ".zip";
	constexpr auto SHA256_ASSET_NAME = "sha256sum.txt";
}

update_manager::update_manager(QObject* parent, std::shared_ptr<gui_settings> gui_settings)
	: QObject(parent), m_gui_settings(std::move(gui_settings))
{
}

void update_manager::check_for_updates(bool automatic, bool check_only, bool auto_accept, QWidget* parent)
{
	update_log.notice("Checking for updates: automatic=%d, check_only=%d, auto_accept=%d", automatic, check_only, auto_accept);

	m_update_message.clear();
	m_release_notes.clear();

	if (automatic)
	{
		// Don't check for updates on local builds
		if (rpcs3::is_local_build())
		{
			update_log.notice("Skipped automatic update check: this is a local build");
			return;
		}
	}

	m_parent     = parent;
	m_downloader = new downloader(parent);

	// GitHub API requires a User-Agent header
	m_downloader->set_user_agent("RPCS3-Mac-Build-Updater");

	connect(m_downloader, &downloader::signal_download_error, this, [this, automatic](const QString& /*error*/)
	{
		if (!automatic)
		{
			QMessageBox::warning(m_parent, tr("Auto-updater"), tr("An error occurred during the auto-updating process.\nCheck the log for more information."));
		}
	});

	connect(m_downloader, &downloader::signal_download_finished, this, [this, automatic, check_only, auto_accept](const QByteArray& data)
	{
		const bool result = handle_github_json(automatic, check_only, auto_accept, data);

		if (!result)
		{
			m_downloader->close_progress_dialog();

			if (!automatic)
			{
				QMessageBox::warning(m_parent, tr("Auto-updater"), tr("An error occurred during the auto-updating process.\nCheck the log for more information."));
			}
		}

		Q_EMIT signal_update_available(result && !m_update_message.isEmpty());
	});

	m_downloader->start(GITHUB_API_URL, true, !automatic, tr("Checking For Updates"), true);
}

bool update_manager::handle_github_json(bool automatic, bool check_only, bool auto_accept, const QByteArray& data)
{
	update_log.notice("Download of GitHub release info finished. automatic=%d, check_only=%d, auto_accept=%d", automatic, check_only, auto_accept);

	const QJsonObject json_data = QJsonDocument::fromJson(data).object();

	// Check for GitHub API errors (rate limiting, not found, etc.)
	if (json_data.contains("message"))
	{
		const std::string message = json_data["message"].toString().toStdString();
		update_log.error("GitHub API error: %s", message);

		if (!automatic)
		{
			if (message.find("rate limit") != std::string::npos)
			{
				QMessageBox::warning(m_parent, tr("Auto-updater"), tr("GitHub API rate limit reached.\nPlease wait a while and try again."));
			}
		}
		return false;
	}

	// Extract release info
	const QString tag_name = json_data["tag_name"].toString();
	if (tag_name.isEmpty())
	{
		update_log.error("GitHub JSON does not contain tag_name");
		return false;
	}

	m_release_tag = tag_name.toStdString();
	const std::string current_tag(rpcs3::get_release_tag());

	update_log.notice("Current tag: '%s', latest tag: '%s'", current_tag, m_release_tag);

	// Check if we're up to date
	if (m_release_tag == current_tag)
	{
		update_log.success("RPCS3 is up to date!");
		m_downloader->close_progress_dialog();

		if (!automatic)
			QMessageBox::information(m_parent, tr("Auto-updater"), tr("Your version is already up to date!"));

		return true;
	}

	// Find the zip and sha256sum assets
	const QJsonArray assets = json_data["assets"].toArray();
	bool found_zip = false;
	bool found_sha256 = false;

	for (const QJsonValue& asset : assets)
	{
		const QJsonObject obj = asset.toObject();
		const QString name = obj["name"].toString();
		const QString download_url = obj["browser_download_url"].toString();

		if (name.endsWith(ZIP_ASSET_SUFFIX) && !found_zip)
		{
			m_request_url = download_url.toStdString();
			m_expected_size = static_cast<u64>(obj["size"].toDouble());
			found_zip = true;
			update_log.notice("Found zip asset: %s (size: %llu)", m_request_url, m_expected_size);
		}
		else if (name == SHA256_ASSET_NAME)
		{
			m_sha256_url = download_url.toStdString();
			found_sha256 = true;
			update_log.notice("Found sha256sum asset: %s", m_sha256_url);
		}
	}

	if (!found_zip)
	{
		update_log.error("No zip asset found in release '%s'", m_release_tag);
		return false;
	}

	if (!found_sha256)
	{
		update_log.error("No sha256sum.txt asset found in release '%s'", m_release_tag);
		return false;
	}

	// Validate URL prefix
	if (!m_request_url.starts_with(GITHUB_DOWNLOAD_PREFIX))
	{
		update_log.fatal("Bad url: %s", m_request_url);
		return false;
	}

	// Build update message
	m_new_version = m_release_tag;
	m_old_version = current_tag.empty() || current_tag == "unknown"
		? fmt::format("%s-%s-%s", rpcs3::get_full_branch(), rpcs3::get_branch(), rpcs3::get_version().to_string())
		: current_tag;

	m_update_message = tr("A new version of RPCS3 is available!<br><br>Current version: %0<br>Latest version: %1<br><br>Do you want to update?")
		.arg(QString::fromStdString(m_old_version))
		.arg(QString::fromStdString(m_new_version));

	// Store release notes from the body field
	if (json_data.contains("body") && json_data["body"].isString())
	{
		m_release_notes = json_data["body"].toString().toStdString();
	}

	update_log.notice("Update found: %s -> %s", m_old_version, m_new_version);

	if (!auto_accept)
	{
		// Check if user has skipped this version
		if (automatic && m_gui_settings->GetValue(gui::ib_skip_version).toString() == QString::fromStdString(m_new_version))
		{
			update_log.notice("Skipping automatic update notification for version '%s' due to user preference", m_new_version);
			m_downloader->close_progress_dialog();
			return true;
		}
	}

	if (check_only)
	{
		update_log.notice("Update postponed. Check only is active");
		m_downloader->close_progress_dialog();
		return true;
	}

	update(auto_accept);
	return true;
}

void update_manager::update(bool auto_accept)
{
	update_log.notice("Updating with auto_accept=%d", auto_accept);

	ensure(m_downloader);

	if (!auto_accept)
	{
		if (m_update_message.isEmpty())
		{
			update_log.notice("Aborting update: Update message is empty. Trying again...");
			m_downloader->close_progress_dialog();
			check_for_updates(false, false, false, m_parent);
			return;
		}

		QMessageBox mb(QMessageBox::Icon::Question, tr("Update Available"), m_update_message, QMessageBox::Yes | QMessageBox::No, m_downloader->get_progress_dialog() ? m_downloader->get_progress_dialog() : m_parent);
		mb.setTextFormat(Qt::RichText);
		mb.setCheckBox(new QCheckBox(tr("Don't show again for this version")));

		if (!m_release_notes.empty())
		{
			mb.setInformativeText(tr("To see the release notes, please click \"Show Details\"."));
			mb.setDetailedText(QString::fromStdString(m_release_notes));
		}

		update_log.notice("Asking user for permission to update...");

		if (mb.exec() == QMessageBox::No)
		{
			update_log.notice("Aborting update: User declined update");

			if (mb.checkBox()->isChecked())
			{
				update_log.notice("User requested to skip further automatic update notifications for version '%s'", m_new_version);
				m_gui_settings->SetValue(gui::ib_skip_version, QString::fromStdString(m_new_version));
			}

			m_downloader->close_progress_dialog();
			return;
		}
	}

	if (!Emu.IsStopped())
	{
		update_log.notice("Aborting update: Emulation is running...");
		m_downloader->close_progress_dialog();
		QMessageBox::warning(m_parent, tr("Auto-updater"), tr("Please stop the emulation before trying to update."));
		return;
	}

	// Step 1: Download sha256sum.txt first
	m_downloader->disconnect();

	connect(m_downloader, &downloader::signal_download_error, this, [this](const QString& /*error*/)
	{
		QMessageBox::warning(m_parent, tr("Auto-updater"), tr("An error occurred during the auto-updating process.\nCheck the log for more information."));
	});

	connect(m_downloader, &downloader::signal_download_finished, this, [this, auto_accept](const QByteArray& data)
	{
		if (handle_sha256(data))
		{
			// SHA256 parsed successfully, now download the zip
			download_update_zip(auto_accept);
		}
		else
		{
			m_downloader->close_progress_dialog();
			QMessageBox::warning(m_parent, tr("Auto-updater"), tr("Failed to verify update checksum.\nCheck the log for more information."));
			Q_EMIT signal_update_available(false);
		}
	});

	update_log.notice("Downloading checksum file...");
	m_downloader->start(m_sha256_url, true, true, tr("Verifying Update"), true);
}

bool update_manager::handle_sha256(const QByteArray& data)
{
	const QString content = QString::fromUtf8(data).trimmed();

	if (content.isEmpty())
	{
		update_log.error("sha256sum.txt is empty");
		return false;
	}

	// sha256sum format: "hash  filename" or just "hash"
	// Split on whitespace and take the first token
	const QStringList lines = content.split('\n', Qt::SkipEmptyParts);

	for (const QString& line : lines)
	{
		const QStringList parts = line.trimmed().split(QRegularExpression("\\s+"));
		if (!parts.isEmpty() && parts[0].length() == 64)
		{
			m_expected_hash = parts[0].toLower().toStdString();
			update_log.notice("Expected SHA256: %s", m_expected_hash);
			return true;
		}
	}

	update_log.error("Could not parse SHA256 hash from: %s", content);
	return false;
}

void update_manager::download_update_zip(bool auto_accept)
{
	m_downloader->disconnect();

	connect(m_downloader, &downloader::signal_download_error, this, [this](const QString& /*error*/)
	{
		QMessageBox::warning(m_parent, tr("Auto-updater"), tr("An error occurred during the auto-updating process.\nCheck the log for more information."));
	});

	connect(m_downloader, &downloader::signal_download_finished, this, [this, auto_accept](const QByteArray& data)
	{
		const bool result = handle_zip(data, auto_accept);

		if (!result)
		{
			m_downloader->close_progress_dialog();
			QMessageBox::warning(m_parent, tr("Auto-updater"), tr("An error occurred during the auto-updating process.\nCheck the log for more information."));
		}

		Q_EMIT signal_update_available(false);
	});

	update_log.notice("Downloading update zip...");
	m_downloader->start(m_request_url, true, true, tr("Downloading Update"), true, static_cast<int>(m_expected_size));
}

bool update_manager::handle_zip(const QByteArray& data, bool auto_accept)
{
	update_log.notice("Download of update file finished. Updating rpcs3 with auto_accept=%d", auto_accept);

	m_downloader->update_progress_dialog(tr("Updating RPCS3"));

	if (m_expected_size != static_cast<u64>(data.size()))
	{
		update_log.error("Download size mismatch: %d expected: %d", data.size(), m_expected_size);
		return false;
	}

	if (const std::string res_hash_string = sha256_get_hash(data.data(), data.size(), false);
		m_expected_hash != res_hash_string)
	{
		update_log.error("Hash mismatch: %s expected: %s", res_hash_string, m_expected_hash);
		return false;
	}

#ifdef __APPLE__
	const std::string exe_dir = fs::get_executable_dir();
	const std::string orig_path = fs::get_executable_path();
	const std::string tmpfile_path = fs::get_temp_dir() + "rpcs3_update.zip";

	update_log.notice("Writing temporary update file: %s", tmpfile_path);

	fs::file tmpfile(tmpfile_path, fs::read + fs::write + fs::create + fs::trunc);
	if (!tmpfile)
	{
		update_log.error("Failed to create temporary file: %s", tmpfile_path);
		return false;
	}
	if (tmpfile.write(data.data(), data.size()) != static_cast<u64>(data.size()))
	{
		update_log.error("Failed to write temporary file: %s", tmpfile_path);
		return false;
	}
	tmpfile.close();

	// Create temp folder for extracting the new app
	const std::string tmp_folder = fs::get_temp_dir() + "rpcs3_new/";

	// Clean up any previous extraction
	if (fs::is_dir(tmp_folder))
	{
		fs::remove_all(tmp_folder);
	}
	fs::create_dir(tmp_folder);

	update_log.notice("Extracting update zip with ditto: %s -> %s", tmpfile_path, tmp_folder);

	// Use ditto to extract the zip (native macOS, preserves code signatures, symlinks, extended attributes)
	QProcess ditto_process;
	ditto_process.setProgram("/usr/bin/ditto");
	ditto_process.setArguments({"-xk", QString::fromStdString(tmpfile_path), QString::fromStdString(tmp_folder)});
	ditto_process.start();
	ditto_process.waitForFinished(120000); // 2 minute timeout

	if (ditto_process.exitCode() != 0)
	{
		const std::string ditto_error = ditto_process.readAllStandardError().toStdString();
		update_log.error("ditto extraction failed (exit code %d): %s", ditto_process.exitCode(), ditto_error);
		return false;
	}

	update_log.success("Update extracted successfully!");

	// Clean up the temp zip file
	fs::remove_file(tmpfile_path);

#else
	update_log.error("Auto-update is only supported on macOS for this build");
	return false;
#endif

	m_downloader->close_progress_dialog();

	// Add new version to log file
	if (fs::file update_file{fs::get_config_dir() + "update_history.log", fs::create + fs::write + fs::append})
	{
		const std::string update_time = QDateTime::currentDateTime().toString("yyyy/MM/dd hh:mm:ss").toStdString();
		const std::string entry = fmt::format("%s: Updated from \"%s\" to \"%s\"", update_time, m_old_version, m_new_version);
		update_file.write(fmt::format("%s\n", entry));
		update_log.notice("Added entry '%s' to update_history.log", entry);
	}
	else
	{
		update_log.error("Failed to append version to update_history.log");
	}

	if (!auto_accept)
	{
		m_gui_settings->ShowInfoBox(tr("Auto-updater"), tr("Update successful!<br>RPCS3 will now restart.<br>"), gui::ib_restart_hint, m_parent);
		m_gui_settings->sync();
	}

	Emu.GracefulShutdown(false);
	Emu.CleanUp();

#ifdef __APPLE__
	// Execute helper script to replace the app and relaunch
	const std::string helper_script = fmt::format("%s/Contents/Resources/update_helper.sh", orig_path);
	const std::string extracted_app = fmt::format("%s/RPCS3.app", tmp_folder);
	update_log.notice("Executing update helper script: '%s %s %s'", helper_script, extracted_app, orig_path);
	const int ret = execl(helper_script.c_str(), helper_script.c_str(), extracted_app.c_str(), orig_path.c_str(), nullptr);
	if (ret == -1)
	{
		update_log.error("Relaunching failed with result: %d(%s)", ret, strerror(errno));
		return false;
	}
#endif

	return true;
}
