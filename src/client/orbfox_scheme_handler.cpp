#include "orbfox_scheme_handler.h"
#include "bookmark_storage.h"
#include "download_manager.h"
#include "history_storage.h"
#include "session_storage.h"
#include "settings_storage.h"
#include "utils/filesystem_utils.h"
#include "utils/json_utils.h"
#include "version.h"

#include "include/cef_parser.h"

#include <sstream>

namespace {

// Settings page HTML - matches OrbFox dark theme
const char* kSettingsPageHtml = R"HTML(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>Settings - OrbFox</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }

        body {
            background: #1c1c1e;
            color: #e5e5e5;
            font-family: -apple-system, BlinkMacSystemFont, 'SF Pro Text', 'Helvetica Neue', sans-serif;
            font-size: 13px;
            line-height: 1.5;
        }

        .container {
            display: flex;
            min-height: 100vh;
        }

        /* Sidebar */
        .sidebar {
            width: 200px;
            background: #141417;
            padding: 24px 12px;
            border-right: 1px solid #404045;
        }

        .sidebar h1 {
            font-size: 18px;
            font-weight: 600;
            margin-bottom: 24px;
            padding: 0 12px;
            color: #e5e5e5;
        }

        .sidebar a {
            display: block;
            padding: 8px 12px;
            color: #999999;
            text-decoration: none;
            border-radius: 6px;
            margin-bottom: 4px;
            transition: all 0.15s ease;
        }

        .sidebar a:hover {
            background: #2e2e33;
            color: #e5e5e5;
        }

        .sidebar a.active {
            background: #007aff;
            color: white;
        }

        /* Content */
        .content {
            flex: 1;
            padding: 32px 48px;
            max-width: 800px;
        }

        .section {
            display: none;
            animation: fadeIn 0.2s ease;
        }

        .section.active {
            display: block;
        }

        @keyframes fadeIn {
            from { opacity: 0; transform: translateY(4px); }
            to { opacity: 1; transform: translateY(0); }
        }

        .section h2 {
            font-size: 22px;
            font-weight: 600;
            margin-bottom: 24px;
            color: #e5e5e5;
        }

        .setting-group {
            margin-bottom: 32px;
        }

        .setting-row {
            display: flex;
            align-items: center;
            justify-content: space-between;
            padding: 16px 0;
            border-bottom: 1px solid #2e2e33;
        }

        .setting-row:last-child {
            border-bottom: none;
        }

        .setting-label {
            flex: 1;
        }

        .setting-label h3 {
            font-size: 14px;
            font-weight: 500;
            color: #e5e5e5;
            margin-bottom: 4px;
        }

        .setting-label p {
            font-size: 12px;
            color: #999999;
        }

        .setting-control {
            flex-shrink: 0;
            margin-left: 24px;
            min-width: 300px;
            display: flex;
            justify-content: flex-end;
        }

        /* Text inputs */
        input[type="text"],
        input[type="url"] {
            width: 300px;
            padding: 8px 12px;
            background: #2e2e33;
            border: 1px solid #404045;
            border-radius: 6px;
            color: #e5e5e5;
            font-size: 13px;
            font-family: inherit;
            transition: border-color 0.15s ease;
        }

        input[type="text"]:focus,
        input[type="url"]:focus {
            outline: none;
            border-color: #007aff;
        }

        input[type="text"]::placeholder,
        input[type="url"]::placeholder {
            color: #666666;
        }

        /* Toggle switch */
        .toggle {
            position: relative;
            width: 44px;
            height: 24px;
        }

        .toggle input {
            opacity: 0;
            width: 0;
            height: 0;
        }

        .toggle-slider {
            position: absolute;
            cursor: pointer;
            top: 0;
            left: 0;
            right: 0;
            bottom: 0;
            background: #555560;
            border: 1px solid #666670;
            border-radius: 24px;
            transition: 0.2s;
        }

        .toggle-slider:before {
            position: absolute;
            content: "";
            height: 20px;
            width: 20px;
            left: 2px;
            bottom: 2px;
            background: white;
            border-radius: 50%;
            transition: 0.2s;
        }

        .toggle input:checked + .toggle-slider {
            background: #007aff;
        }

        .toggle input:checked + .toggle-slider:before {
            transform: translateX(20px);
        }

        /* Radio buttons */
        .radio-group {
            display: flex;
            flex-direction: column;
            gap: 12px;
        }

        .radio-option {
            display: flex;
            align-items: center;
            cursor: pointer;
        }

        .radio-option input {
            display: none;
        }

        .radio-circle {
            width: 18px;
            height: 18px;
            border: 2px solid #404045;
            border-radius: 50%;
            margin-right: 10px;
            display: flex;
            align-items: center;
            justify-content: center;
            transition: border-color 0.15s ease;
        }

        .radio-option input:checked + .radio-circle {
            border-color: #007aff;
        }

        .radio-circle:after {
            content: "";
            width: 10px;
            height: 10px;
            background: #007aff;
            border-radius: 50%;
            opacity: 0;
            transition: opacity 0.15s ease;
        }

        .radio-option input:checked + .radio-circle:after {
            opacity: 1;
        }

        .radio-label {
            color: #e5e5e5;
            font-size: 13px;
        }

        /* Buttons */
        .btn {
            padding: 8px 16px;
            border-radius: 6px;
            font-size: 13px;
            font-weight: 500;
            cursor: pointer;
            border: none;
            transition: all 0.15s ease;
        }

        .btn-primary {
            background: #007aff;
            color: white;
        }

        .btn-primary:hover {
            background: #1a8cff;
        }

        .btn-danger {
            background: #e64d4d;
            color: white;
        }

        .btn-danger:hover {
            background: #ff5555;
        }

        .btn-secondary {
            background: #3a3a3c;
            color: #e5e5e5;
            padding: 6px 12px;
            font-size: 12px;
        }

        .btn-secondary:hover {
            background: #48484a;
        }

        .btn-secondary.active {
            background: #007aff;
            color: white;
        }

        .url-input-group {
            display: flex;
            flex-direction: column;
            gap: 8px;
        }

        .quick-options {
            display: flex;
            gap: 8px;
        }

        .btn-secondary {
            background: #2e2e33;
            color: #e5e5e5;
            border: 1px solid #404045;
        }

        .btn-secondary:hover {
            background: #38383d;
        }

        /* Toast notification */
        .toast {
            position: fixed;
            bottom: 24px;
            right: 24px;
            padding: 12px 20px;
            background: #2e2e33;
            border: 1px solid #404045;
            border-radius: 8px;
            color: #e5e5e5;
            font-size: 13px;
            opacity: 0;
            transform: translateY(10px);
            transition: all 0.2s ease;
            pointer-events: none;
        }

        .toast.show {
            opacity: 1;
            transform: translateY(0);
        }

        .toast.success {
            border-color: #4d9b5c;
        }

        /* About section */
        .about-info {
            background: #2e2e33;
            padding: 20px;
            border-radius: 8px;
            margin-bottom: 20px;
        }

        .about-info h3 {
            font-size: 16px;
            font-weight: 600;
            margin-bottom: 12px;
            color: #e5e5e5;
        }

        .about-row {
            display: flex;
            justify-content: space-between;
            padding: 8px 0;
            border-bottom: 1px solid #404045;
        }

        .about-row:last-child {
            border-bottom: none;
        }

        .about-label {
            color: #999999;
        }

        .about-value {
            color: #e5e5e5;
        }
    </style>
</head>
<body>
    <div class="container">
        <nav class="sidebar">
            <h1>Settings</h1>
            <a href="#general" class="active" data-section="general">General</a>
            <a href="#privacy" data-section="privacy">Privacy</a>
            <a href="#downloads" data-section="downloads">Downloads</a>
            <a href="#about" data-section="about">About</a>
        </nav>
        <main class="content">
            <!-- General Section -->
            <section id="general" class="section active">
                <h2>General</h2>
                <div class="setting-group">
                    <div class="setting-row">
                        <div class="setting-label">
                            <h3>Homepage</h3>
                            <p>The page that opens when you click the home button</p>
                        </div>
                        <div class="setting-control">
                            <div class="url-input-group">
                                <input type="url" id="homepage_url" placeholder="orbfox://bookmarks">
                                <div class="quick-options">
                                    <button class="btn btn-secondary" data-target="homepage_url" data-url="orbfox://bookmarks">Bookmarks</button>
                                    <button class="btn btn-secondary" data-target="homepage_url" data-url="about:blank">Blank</button>
                                </div>
                            </div>
                        </div>
                    </div>
                    <div class="setting-row">
                        <div class="setting-label">
                            <h3>New Tab Page</h3>
                            <p>The page that opens when you create a new tab</p>
                        </div>
                        <div class="setting-control">
                            <div class="url-input-group">
                                <input type="url" id="new_tab_url" placeholder="orbfox://bookmarks">
                                <div class="quick-options">
                                    <button class="btn btn-secondary" data-target="new_tab_url" data-url="orbfox://bookmarks">Bookmarks</button>
                                    <button class="btn btn-secondary" data-target="new_tab_url" data-url="about:blank">Blank</button>
                                </div>
                            </div>
                        </div>
                    </div>
                    <div class="setting-row">
                        <div class="setting-label">
                            <h3>On Startup</h3>
                            <p>Choose what happens when OrbFox starts</p>
                        </div>
                        <div class="setting-control">
                            <div class="radio-group">
                                <label class="radio-option">
                                    <input type="radio" name="restore_session" value="false">
                                    <span class="radio-circle"></span>
                                    <span class="radio-label">Open new tab page</span>
                                </label>
                                <label class="radio-option">
                                    <input type="radio" name="restore_session" value="true" checked>
                                    <span class="radio-circle"></span>
                                    <span class="radio-label">Restore previous session</span>
                                </label>
                            </div>
                        </div>
                    </div>
                </div>
            </section>

            <!-- Privacy Section -->
            <section id="privacy" class="section">
                <h2>Privacy</h2>
                <div class="setting-group">
                    <div class="setting-row">
                        <div class="setting-label">
                            <h3>Tracking Protection</h3>
                            <p>Block ads and trackers for faster, more private browsing</p>
                        </div>
                        <div class="setting-control">
                            <label class="toggle">
                                <input type="checkbox" id="tracking_protection" checked>
                                <span class="toggle-slider"></span>
                            </label>
                        </div>
                    </div>
                    <div class="setting-row">
                        <div class="setting-label">
                            <h3>Clear Browsing Data</h3>
                            <p>Clear history, downloads, and session data</p>
                        </div>
                        <div class="setting-control">
                            <button class="btn btn-danger" id="clear_data_btn">Clear Data</button>
                        </div>
                    </div>
                </div>
            </section>

            <!-- Downloads Section -->
            <section id="downloads" class="section">
                <h2>Downloads</h2>
                <div class="setting-group">
                    <div class="setting-row">
                        <div class="setting-label">
                            <h3>Download Location</h3>
                            <p>Where downloaded files are saved</p>
                        </div>
                        <div class="setting-control">
                            <input type="text" id="download_path" placeholder="~/Downloads">
                        </div>
                    </div>
                    <div class="setting-row">
                        <div class="setting-label">
                            <h3>Ask Before Downloading</h3>
                            <p>Show confirmation dialog before starting downloads</p>
                        </div>
                        <div class="setting-control">
                            <label class="toggle">
                                <input type="checkbox" id="ask_before_download" checked>
                                <span class="toggle-slider"></span>
                            </label>
                        </div>
                    </div>
                </div>
            </section>

            <!-- About Section -->
            <section id="about" class="section">
                <h2>About OrbFox</h2>
                <div class="about-info">
                    <h3>Browser Information</h3>
                    <div class="about-row">
                        <span class="about-label">Version</span>
                        <span class="about-value">1.0.0</span>
                    </div>
                    <div class="about-row">
                        <span class="about-label">Engine</span>
                        <span class="about-value">Chromium Embedded Framework (CEF)</span>
                    </div>
                    <div class="about-row">
                        <span class="about-label">Platform</span>
                        <span class="about-value">macOS</span>
                    </div>
                </div>
                <p style="color: #999999; font-size: 12px;">
                    OrbFox is a custom browser with a Vivaldi-style sidebar interface.
                </p>
            </section>
        </main>
    </div>

    <div class="toast" id="toast">Settings saved</div>

    <script>
        let settings = {};
        let saveTimeout = null;

        // Load settings on page load
        async function loadSettings() {
            try {
                const response = await fetch('orbfox://settings/api/get');
                settings = await response.json();
                applySettingsToUI();
            } catch (e) {
                console.error('Failed to load settings:', e);
            }
        }

        // Apply loaded settings to form elements
        function applySettingsToUI() {
            document.getElementById('homepage_url').value = settings.homepage_url || '';
            document.getElementById('new_tab_url').value = settings.new_tab_url || '';
            updateQuickOptionButtons();
            document.getElementById('tracking_protection').checked = settings.tracking_protection !== false;
            document.getElementById('download_path').value = settings.download_path || '';
            document.getElementById('ask_before_download').checked = settings.ask_before_download !== false;

            // Radio buttons for restore_session
            const radios = document.querySelectorAll('input[name="restore_session"]');
            radios.forEach(radio => {
                radio.checked = (radio.value === 'true') === (settings.restore_session !== false);
            });
        }

        // Save settings with debounce
        function saveSettings() {
            if (saveTimeout) clearTimeout(saveTimeout);
            saveTimeout = setTimeout(async () => {
                try {
                    await fetch('orbfox://settings/api/set', {
                        method: 'POST',
                        headers: { 'Content-Type': 'application/json' },
                        body: JSON.stringify(settings)
                    });
                    showToast('Settings saved');
                } catch (e) {
                    console.error('Failed to save settings:', e);
                    showToast('Failed to save', true);
                }
            }, 300);
        }

        // Show toast notification
        function showToast(message, isError = false) {
            const toast = document.getElementById('toast');
            toast.textContent = message;
            toast.classList.toggle('success', !isError);
            toast.classList.add('show');
            setTimeout(() => toast.classList.remove('show'), 2000);
        }

        // Section navigation
        document.querySelectorAll('.sidebar a').forEach(link => {
            link.addEventListener('click', (e) => {
                e.preventDefault();
                const sectionId = link.getAttribute('data-section');

                // Update active states
                document.querySelectorAll('.sidebar a').forEach(l => l.classList.remove('active'));
                link.classList.add('active');

                document.querySelectorAll('.section').forEach(s => s.classList.remove('active'));
                document.getElementById(sectionId).classList.add('active');
            });
        });

        // Input handlers
        document.getElementById('homepage_url').addEventListener('input', (e) => {
            settings.homepage_url = e.target.value;
            saveSettings();
        });

        document.getElementById('new_tab_url').addEventListener('input', (e) => {
            settings.new_tab_url = e.target.value;
            saveSettings();
            updateQuickOptionButtons();
        });

        // Quick option buttons for URL fields
        function updateQuickOptionButtons() {
            document.querySelectorAll('.quick-options button').forEach(btn => {
                const target = btn.getAttribute('data-target');
                const url = btn.getAttribute('data-url');
                const input = document.getElementById(target);
                btn.classList.toggle('active', input && input.value === url);
            });
        }

        document.querySelectorAll('.quick-options button').forEach(btn => {
            btn.addEventListener('click', (e) => {
                e.preventDefault();
                const target = btn.getAttribute('data-target');
                const url = btn.getAttribute('data-url');
                const input = document.getElementById(target);
                if (input) {
                    input.value = url;
                    settings[target] = url;
                    saveSettings();
                    updateQuickOptionButtons();
                }
            });
        });

        document.getElementById('homepage_url').addEventListener('input', () => updateQuickOptionButtons());

        document.querySelectorAll('input[name="restore_session"]').forEach(radio => {
            radio.addEventListener('change', (e) => {
                settings.restore_session = e.target.value === 'true';
                saveSettings();
            });
        });

        document.getElementById('tracking_protection').addEventListener('change', (e) => {
            settings.tracking_protection = e.target.checked;
            saveSettings();
        });

        document.getElementById('download_path').addEventListener('input', (e) => {
            settings.download_path = e.target.value;
            saveSettings();
        });

        document.getElementById('ask_before_download').addEventListener('change', (e) => {
            settings.ask_before_download = e.target.checked;
            saveSettings();
        });

        // Clear data button
        document.getElementById('clear_data_btn').addEventListener('click', async () => {
            if (confirm('This will clear all browsing history, downloads, and session data. Continue?')) {
                try {
                    await fetch('orbfox://settings/api/clear', { method: 'POST' });
                    showToast('Browsing data cleared');
                } catch (e) {
                    showToast('Failed to clear data', true);
                }
            }
        });

        // Initialize
        loadSettings();
    </script>
</body>
</html>
)HTML";

// Bookmarks page HTML - clean new tab page showing bookmarks
const char* kBookmarksPageHtml = R"HTML(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>New Tab</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }

        body {
            background: #1c1c1e;
            color: #e5e5e5;
            font-family: -apple-system, BlinkMacSystemFont, 'SF Pro Text', 'Helvetica Neue', sans-serif;
            font-size: 14px;
            line-height: 1.5;
            min-height: 100vh;
            padding: 48px;
        }

        .container {
            max-width: 900px;
            margin: 0 auto;
        }

        h1 {
            font-size: 28px;
            font-weight: 600;
            margin-bottom: 32px;
            color: #ffffff;
        }

        .bookmarks-grid {
            display: grid;
            grid-template-columns: repeat(auto-fill, minmax(180px, 1fr));
            gap: 16px;
        }

        .bookmark-card {
            background: #2c2c2e;
            border-radius: 12px;
            padding: 16px;
            text-decoration: none;
            color: #e5e5e5;
            transition: all 0.15s ease;
            display: flex;
            flex-direction: column;
            align-items: center;
            text-align: center;
            gap: 12px;
        }

        .bookmark-card:hover {
            background: #3a3a3c;
            transform: translateY(-2px);
        }

        .bookmark-icon {
            width: 48px;
            height: 48px;
            background: #48484a;
            border-radius: 10px;
            display: flex;
            align-items: center;
            justify-content: center;
            font-size: 20px;
            font-weight: 600;
            color: #007aff;
        }

        .bookmark-icon img {
            width: 32px;
            height: 32px;
            border-radius: 4px;
        }

        .bookmark-title {
            font-size: 13px;
            font-weight: 500;
            overflow: hidden;
            text-overflow: ellipsis;
            white-space: nowrap;
            width: 100%;
        }

        .bookmark-url {
            font-size: 11px;
            color: #8e8e93;
            overflow: hidden;
            text-overflow: ellipsis;
            white-space: nowrap;
            width: 100%;
        }

        .empty-state {
            text-align: center;
            padding: 64px 24px;
            color: #8e8e93;
        }

        .empty-state h2 {
            font-size: 20px;
            font-weight: 500;
            margin-bottom: 12px;
            color: #e5e5e5;
        }

        .empty-state p {
            font-size: 14px;
        }

        .folder-section {
            margin-bottom: 32px;
        }

        .folder-header {
            font-size: 16px;
            font-weight: 600;
            color: #8e8e93;
            margin-bottom: 16px;
            text-transform: uppercase;
            letter-spacing: 0.5px;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>Bookmarks</h1>
        <div id="bookmarks-container">
            <div class="empty-state">
                <h2>No bookmarks yet</h2>
                <p>Press ⌘+D to bookmark the current page</p>
            </div>
        </div>
    </div>

    <script>
        async function loadBookmarks() {
            try {
                const response = await fetch('orbfox://bookmarks/api/list');
                const data = await response.json();
                renderBookmarks(data.bookmarks || []);
            } catch (e) {
                console.error('Failed to load bookmarks:', e);
            }
        }

        function getFaviconUrl(url) {
            try {
                const urlObj = new URL(url);
                return `https://www.google.com/s2/favicons?domain=${urlObj.hostname}&sz=64`;
            } catch {
                return null;
            }
        }

        function getInitial(title, url) {
            if (title && title.length > 0) {
                return title[0].toUpperCase();
            }
            try {
                const urlObj = new URL(url);
                return urlObj.hostname[0].toUpperCase();
            } catch {
                return '?';
            }
        }

        function getDomain(url) {
            try {
                const urlObj = new URL(url);
                return urlObj.hostname.replace('www.', '');
            } catch {
                return url;
            }
        }

        function renderBookmarks(bookmarks) {
            const container = document.getElementById('bookmarks-container');

            if (bookmarks.length === 0) {
                container.innerHTML = `
                    <div class="empty-state">
                        <h2>No bookmarks yet</h2>
                        <p>Press ⌘+D to bookmark the current page</p>
                    </div>
                `;
                return;
            }

            // Group by folder
            const folders = {};
            const rootBookmarks = [];

            bookmarks.forEach(bm => {
                if (bm.folder && bm.folder.length > 0) {
                    if (!folders[bm.folder]) {
                        folders[bm.folder] = [];
                    }
                    folders[bm.folder].push(bm);
                } else {
                    rootBookmarks.push(bm);
                }
            });

            let html = '';

            // Render root bookmarks first
            if (rootBookmarks.length > 0) {
                html += '<div class="bookmarks-grid">';
                rootBookmarks.forEach(bm => {
                    html += renderBookmarkCard(bm);
                });
                html += '</div>';
            }

            // Render folders
            Object.keys(folders).sort().forEach(folder => {
                html += `
                    <div class="folder-section">
                        <div class="folder-header">${escapeHtml(folder)}</div>
                        <div class="bookmarks-grid">
                            ${folders[folder].map(bm => renderBookmarkCard(bm)).join('')}
                        </div>
                    </div>
                `;
            });

            container.innerHTML = html;
        }

        function renderBookmarkCard(bm) {
            const faviconUrl = getFaviconUrl(bm.url);
            const initial = getInitial(bm.title, bm.url);
            const domain = getDomain(bm.url);
            const title = bm.title || domain;

            return `
                <a href="${escapeHtml(bm.url)}" class="bookmark-card">
                    <div class="bookmark-icon">
                        ${faviconUrl
                            ? `<img src="${faviconUrl}" onerror="this.parentElement.innerHTML='${initial}'">`
                            : initial
                        }
                    </div>
                    <div class="bookmark-title">${escapeHtml(title)}</div>
                    <div class="bookmark-url">${escapeHtml(domain)}</div>
                </a>
            `;
        }

        function escapeHtml(text) {
            const div = document.createElement('div');
            div.textContent = text;
            return div.innerHTML;
        }

        loadBookmarks();
    </script>
</body>
</html>
)HTML";

}  // namespace

CefRefPtr<CefResourceHandler> OrbfoxSchemeHandlerFactory::Create(
    CefRefPtr<CefBrowser> browser,
    CefRefPtr<CefFrame> frame,
    const CefString& scheme_name,
    CefRefPtr<CefRequest> request) {
    return new OrbfoxResourceHandler();
}

OrbfoxResourceHandler::OrbfoxResourceHandler() = default;

bool OrbfoxResourceHandler::Open(CefRefPtr<CefRequest> request,
                                  bool& handle_request,
                                  CefRefPtr<CefCallback> callback) {
    handle_request = true;

    std::string url = request->GetURL().ToString();
    std::string method = request->GetMethod().ToString();

    // Parse the URL path
    // orbfox://settings -> path = "/settings" or just "settings"
    size_t scheme_end = url.find("://");
    std::string path;
    if (scheme_end != std::string::npos) {
        path = url.substr(scheme_end + 3);  // Skip "orbfox://"
    }

    // Remove query string if present
    size_t query_pos = path.find('?');
    if (query_pos != std::string::npos) {
        path = path.substr(0, query_pos);
    }

    // Check origin for state-changing requests (CSRF protection)
    auto IsValidOrigin = [&request]() -> bool {
        CefString origin = request->GetHeaderByName("Origin");
        CefString referer = request->GetHeaderByName("Referer");
        std::string origin_str = origin.ToString();
        std::string referer_str = referer.ToString();

        // Allow if origin is orbfox:// or empty (same-origin requests may not have Origin)
        if (origin_str.empty() || origin_str.find("orbfox://") == 0) {
            // Also check referer if present
            if (referer_str.empty() || referer_str.find("orbfox://") == 0) {
                return true;
            }
        }
        return false;
    };

    // Route the request
    if (path == "settings" || path == "settings/") {
        HandleSettingsPage();
    } else if (path == "settings/api/get") {
        HandleSettingsApiGet();
    } else if (path == "settings/api/set" && method == "POST") {
        // CSRF check for state-changing endpoint
        if (!IsValidOrigin()) {
            data_ = R"({"success": false, "error": "Invalid origin"})";
            mime_type_ = "application/json";
            status_code_ = 403;
            return true;
        }
        // Get POST data
        CefRefPtr<CefPostData> post_data = request->GetPostData();
        std::string post_body;
        if (post_data && post_data->GetElementCount() > 0) {
            CefPostData::ElementVector elements;
            post_data->GetElements(elements);
            if (!elements.empty()) {
                size_t size = elements[0]->GetBytesCount();
                post_body.resize(size);
                elements[0]->GetBytes(size, &post_body[0]);
            }
        }
        HandleSettingsApiSet(post_body);
    } else if (path == "settings/api/clear" && method == "POST") {
        // CSRF check for state-changing endpoint
        if (!IsValidOrigin()) {
            data_ = R"({"success": false, "error": "Invalid origin"})";
            mime_type_ = "application/json";
            status_code_ = 403;
            return true;
        }
        // Clear browsing data
        bool success = true;

        // Clear history
        HistoryStorage* history = GetHistoryStorage();
        if (history) {
            history->ClearAllHistory();
        }

        // Clear completed/canceled downloads from download manager
        DownloadManager::GetInstance().ClearCompleted();

        // Clear session (will start fresh on next launch)
        SessionStorage::Clear();

        data_ = success ? R"({"success": true})" : R"({"success": false})";
        mime_type_ = "application/json";
        status_code_ = 200;
    } else if (path == "bookmarks" || path == "bookmarks/") {
        HandleBookmarksPage();
    } else if (path == "bookmarks/api/list") {
        HandleBookmarksApiList();
    } else {
        HandleNotFound(path);
    }

    return true;
}

void OrbfoxResourceHandler::GetResponseHeaders(CefRefPtr<CefResponse> response,
                                                int64_t& response_length,
                                                CefString& redirectUrl) {
    response->SetMimeType(mime_type_);
    response->SetStatus(status_code_);

    // No CORS headers - orbfox:// is same-origin for orbfox:// pages
    // This prevents external websites from accessing the settings API

    response_length = static_cast<int64_t>(data_.size());
}

bool OrbfoxResourceHandler::Read(void* data_out,
                                  int bytes_to_read,
                                  int& bytes_read,
                                  CefRefPtr<CefResourceReadCallback> callback) {
    if (offset_ >= data_.size()) {
        bytes_read = 0;
        return false;
    }

    size_t remaining = data_.size() - offset_;
    size_t to_copy = std::min(static_cast<size_t>(bytes_to_read), remaining);

    memcpy(data_out, data_.data() + offset_, to_copy);
    offset_ += to_copy;
    bytes_read = static_cast<int>(to_copy);

    return true;
}

void OrbfoxResourceHandler::HandleSettingsPage() {
    data_ = kSettingsPageHtml;

    // Replace version placeholder with actual version from version.h
    const std::string version_placeholder = "1.0.0</span>";
    const std::string version_replacement = std::string(ORBFOX_VERSION_STRING) + "</span>";
    size_t pos = data_.find(version_placeholder);
    if (pos != std::string::npos) {
        data_.replace(pos, version_placeholder.length(), version_replacement);
    }

    mime_type_ = "text/html";
    status_code_ = 200;
}

void OrbfoxResourceHandler::HandleSettingsApiGet() {
    data_ = SettingsStorage::GetInstance().ToJson();
    mime_type_ = "application/json";
    status_code_ = 200;
}

void OrbfoxResourceHandler::HandleSettingsApiSet(const std::string& post_data) {
    if (SettingsStorage::GetInstance().FromJson(post_data)) {
        SettingsStorage::GetInstance().Save();
        data_ = R"({"success": true})";
    } else {
        data_ = R"({"success": false, "error": "Invalid JSON"})";
    }
    mime_type_ = "application/json";
    status_code_ = 200;
}

void OrbfoxResourceHandler::HandleBookmarksPage() {
    data_ = kBookmarksPageHtml;
    mime_type_ = "text/html";
    status_code_ = 200;
}

void OrbfoxResourceHandler::HandleBookmarksApiList() {
    BookmarkStorage* storage = GetBookmarkStorage();
    std::ostringstream ss;
    ss << R"({"bookmarks": [)";

    if (storage) {
        auto bookmarks = storage->GetAllBookmarks();
        bool first = true;
        for (const auto& bm : bookmarks) {
            if (!first) ss << ",";
            first = false;
            ss << R"({"id":)" << bm.id
               << R"(,"url":")" << orbfox::utils::EscapeJsonString(bm.url) << R"(")"
               << R"(,"title":")" << orbfox::utils::EscapeJsonString(bm.title) << R"(")"
               << R"(,"folder":")" << orbfox::utils::EscapeJsonString(bm.folder) << R"("})";
        }
    }

    ss << "]}";
    data_ = ss.str();
    mime_type_ = "application/json";
    status_code_ = 200;
}

void OrbfoxResourceHandler::HandleNotFound(const std::string& path) {
    std::ostringstream ss;
    ss << "<!DOCTYPE html><html><head><title>Not Found</title>"
       << "<style>body{background:#1c1c1e;color:#e5e5e5;font-family:-apple-system,sans-serif;"
       << "display:flex;align-items:center;justify-content:center;height:100vh;margin:0;}"
       << "h1{color:#ff6b6b;}</style></head><body>"
       << "<div><h1>404 Not Found</h1><p>The page <code>" << orbfox::utils::EscapeHtml(path) << "</code> was not found.</p></div>"
       << "</body></html>";
    data_ = ss.str();
    mime_type_ = "text/html";
    status_code_ = 404;
}

void RegisterOrbfoxSchemeHandler() {
    CefRegisterSchemeHandlerFactory("orbfox", "", new OrbfoxSchemeHandlerFactory());
}
