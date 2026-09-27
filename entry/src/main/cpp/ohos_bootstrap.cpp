/*
 * HarmonyOS first-run asset bootstrap.
 *
 * The hdc shell user can't write into the app sandbox, so we let the app pull
 * the game data itself (libcurl is statically linked) from a one-off HTTP
 * server running on the Mac over the LAN. Downloads are resumable, so an
 * interrupted transfer can be retried.
 */
#include <curl/curl.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

// Embedded Mozilla CA bundle (generated from cacert.pem via xxd -i).
extern "C" const unsigned char cacert_pem[];
extern "C" const unsigned int cacert_pem_len;
extern "C" void OhosInput_StartMonitors();

// context.filesDir as seen from inside the process, plus the engine layout
#define DATA_ROOT "/data/storage/el2/base/haps/entry/files/0ad/data"
#define CONFIG_ROOT "/data/storage/el2/base/haps/entry/files/0ad/config"
// Mac dev machine LAN HTTP server root = binaries/data
static const char* HTTP_HOST = "http://192.168.1.32:8000";

static int MkdirP(const std::string& path)
{
	for (size_t i = 1; i < path.size(); ++i)
	{
		if (path[i] == '/')
		{
			std::string sub = path.substr(0, i);
			mkdir(sub.c_str(), 0770);
		}
	}
	return mkdir(path.c_str(), 0770);
}

static long ExistingSize(const std::string& path)
{
	struct stat st;
	if (stat(path.c_str(), &st) != 0)
		return 0;
	return static_cast<long>(st.st_size);
}

static int ProgressCb(void* /*userdata*/, double dlTotal, double dlNow,
                      double /*ulTotal*/, double /*ulNow*/)
{
	if (dlTotal > 0.0)
	{
		double pct = (dlNow + dlTotal > 0.0) ? 100.0 * dlNow / dlTotal : 0.0;
		std::fprintf(stderr, "[bootstrap] %.1f%% (%.0f/%.0f bytes)\n",
		             pct, dlNow, dlTotal);
	}
	return 0;
}

static bool Download(CURL* curl, const std::string& urlRel,
                     const std::string& out, bool forceFull = false)
{
	const std::string url = std::string(HTTP_HOST) + "/" + urlRel;

	const size_t slash = out.find_last_of('/');
	if (slash != std::string::npos)
		MkdirP(out.substr(0, slash));

	// Forced refreshes stage into <out>.new and only swap in on full success,
	// so a failed download can't clobber a working file.
	const std::string dst = forceFull ? (out + ".new") : out;
	if (forceFull)
		std::remove(dst.c_str());

	const long resumeFrom = forceFull ? 0 : ExistingSize(out);

	// Determine total size to decide whether the file is already complete.
	double contentLen = 0.0;
	curl_easy_reset(curl);
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
	curl_easy_setopt(curl, CURLOPT_HEADER, 1L);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
	if (curl_easy_perform(curl) == CURLE_OK)
		curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &contentLen);

	if (!forceFull && contentLen > 0.0 && resumeFrom >= static_cast<long>(contentLen))
	{
		std::fprintf(stderr, "[bootstrap] %s already complete\n", urlRel.c_str());
		return true;
	}

	FILE* fp = std::fopen(dst.c_str(), forceFull ? "wb" : "ab");
	if (!fp)
	{
		std::fprintf(stderr, "[bootstrap] cannot open %s\n", dst.c_str());
		return false;
	}

	curl_easy_reset(curl);
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
	curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, ProgressCb);
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
	if (!forceFull && resumeFrom > 0)
		curl_easy_setopt(curl, CURLOPT_RESUME_FROM_LARGE,
		                 static_cast<curl_off_t>(resumeFrom));

	const CURLcode rc = curl_easy_perform(curl);
	std::fclose(fp);

	if (rc != CURLE_OK)
	{
		std::fprintf(stderr, "[bootstrap] %s failed: %s\n",
		             urlRel.c_str(), curl_easy_strerror(rc));
		if (forceFull)
			std::remove(dst.c_str());
		return false;
	}

	if (forceFull && std::rename(dst.c_str(), out.c_str()) != 0)
	{
		std::fprintf(stderr, "[bootstrap] could not replace %s\n", out.c_str());
		std::remove(dst.c_str());
		return false;
	}

	std::fprintf(stderr, "[bootstrap] %s done\n", urlRel.c_str());
	return true;
}

// Write the embedded CA bundle into the config dir and point OpenSSL/libcurl at
// it. Without this, HTTPS (mod.io, telemetry) and XMPP-TLS (lobby) fail with
// "SSL certificate problem: unable to get local issuer certificate" because the
// device has no OpenSSL-visible system CA store. Must run before any TLS use.
static void SetupCertBundle()
{
	MkdirP(CONFIG_ROOT);
	const std::string path = std::string(CONFIG_ROOT) + "/cacert.pem";
	FILE* fp = std::fopen(path.c_str(), "wb");
	if (fp)
	{
		std::fwrite(cacert_pem, 1, cacert_pem_len, fp);
		std::fclose(fp);
	}
	setenv("SSL_CERT_FILE", path.c_str(), 1);
	setenv("CURL_CA_BUNDLE", path.c_str(), 1);
	std::fprintf(stderr, "[bootstrap] CA bundle installed at %s (%u bytes)\n",
	             path.c_str(), cacert_pem_len);
}

extern "C" void ohos_bootstrap_assets()
{
	SetupCertBundle();
	OhosInput_StartMonitors();

	struct DL { const char* urlRel; const char* out; bool force; };
	static const DL files[] = {
		// user.cfg suppresses the first-run splash/welcome dialog
		{ "user.cfg",
		  CONFIG_ROOT "/user.cfg", false },
		// Refreshed (enabledmods gains the override mod); staged via .new
		{ "config/default.cfg",
		  DATA_ROOT "/config/default.cfg", true },
		{ "mods/mod/mod.zip",
		  DATA_ROOT "/mods/mod/mod.zip", false },
		{ "mods/public/mod.json",
		  DATA_ROOT "/mods/public/mod.json", false },
		{ "mods/public/public.zip",
		  DATA_ROOT "/mods/public/public.zip", false },
		// Small HarmonyOS override mod (network management UI, ...)
		{ "mods/zeroadpatches/mod.json",
		  DATA_ROOT "/mods/zeroadpatches/mod.json", true },
		{ "mods/zeroadpatches/zeroadpatches.zip",
		  DATA_ROOT "/mods/zeroadpatches/zeroadpatches.zip", true },
	};

	curl_global_init(CURL_GLOBAL_ALL);
	CURL* curl = curl_easy_init();
	if (!curl)
	{
		std::fprintf(stderr, "[bootstrap] curl init failed\n");
		return;
	}

	for (const DL& f : files)
	{
		// Retry a few times; the big zip may be interrupted.
		for (int attempt = 0; attempt < 50; ++attempt)
		{
			if (Download(curl, f.urlRel, f.out, f.force))
				break;
			std::fprintf(stderr, "[bootstrap] retry %s (%d)\n",
			             f.urlRel, attempt);
		}
	}

	curl_easy_cleanup(curl);
	std::fprintf(stderr, "[bootstrap] all assets ready\n");
}
