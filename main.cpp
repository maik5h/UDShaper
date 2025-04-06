// Everything needed to make the plugin defined in src/ a CLAP plugin.
// Assigns clap_plugin_descriptor attributes, sets up extensions and defines clap_plugin methods.

#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <fstream>
#include <iostream>
#include <vector>
#include <chrono>
#include "clap/clap.h"
#include "config.h"
#include "src/shapeEditor.h"
#include "src/GUI.h"
#include "src/assets.h"
#include "src/UDShaper.h"

// for debugging (FL Studio does not support logging)
void logToFile(const std::string& message) {
    std::ofstream logFile("C:/Users/mm/Desktop/log.txt", std::ios_base::app);
    if (logFile.is_open()) {
        logFile << message << std::endl;
    } else {
        std::cerr << "Failed to open log file!" << std::endl;
    }
}

// GUI is rendered on the main thread, while the song position is only available on the audio threads.
// This function compares the process corresponding to the thread it is called in with the last process written to the UDShaper object.
// It will overwrite the saved start time if the start time of this process is earlier, this way the true starting time of the audio buffer can be determined. The initial beatPosition is saved together with the start time.
// From this information, the beatPosition can be calculated on the main thread.
//
// Uses Mutex but should be fast enough to not cause major delays in processing. 
void reportPlaybackStatusToMain(UDShaper *plugin, const clap_process_t *process) {
	int samplerate = 44100; // TODO how to get actual sr????

	long processStartTime = getCurrentTime(); // The time at sample 0 of this process.
	long processStopTime = processStartTime + (long)(1000 * process->frames_count / samplerate); // The time at the last sample of this process.
	double processStartPosition = (double)process->transport->song_pos_beats / CLAP_BEATTIME_FACTOR; // The beatPosition at sample 0 of this process.
	bool isPlaying = (process->transport->flags & CLAP_TRANSPORT_IS_PLAYING);

	WaitForSingleObject(plugin->synchProcessStartTime, INFINITE);

	plugin->initBeatPosition = processStartPosition;

	// If this process is earlier than the one that previously wrote to plugin->processStartTime, overwrite the value.
	if (isPlaying && (plugin->startedPlaying > processStartTime || plugin->startedPlaying == 0)) {
		plugin->startedPlaying = processStartTime;
		plugin->currentTempo = process->transport->tempo;
		plugin->hostPlaying = true;
	}
	if (!isPlaying) {
		plugin->startedPlaying = 0;
		plugin->hostPlaying = false;
	}

	ReleaseMutex(plugin->synchProcessStartTime);
}

#ifdef _WIN32
#include <windows.h>
typedef HANDLE Mutex;
#define MutexAcquire(mutex) WaitForSingleObject(mutex, INFINITE)
#define MutexRelease(mutex) ReleaseMutex(mutex)
#define MutexInitialise(mutex) (mutex = CreateMutex(nullptr, FALSE, nullptr))
#define MutexDestroy(mutex) CloseHandle(mutex)
#else
#include <pthread.h>
typedef pthread_mutex_t Mutex;
#define MutexAcquire(mutex) pthread_mutex_lock(&(mutex))
#define MutexRelease(mutex) pthread_mutex_unlock(&(mutex))
#define MutexInitialise(mutex) pthread_mutex_init(&(mutex), nullptr)
#define MutexDestroy(mutex) pthread_mutex_destroy(&(mutex))
#endif

static const clap_plugin_descriptor_t pluginDescriptor = {
	.clap_version = CLAP_VERSION_INIT,
	.id = "maik5h.UDShaper",
	.name = "UDShaper",
	.vendor = "maik5h",
	.url = "https://github.com/maik5h",
	.manual_url = "",
	.support_url = "",
	.version = "1.0.0",
	.description = "Upwards-downwards fully modulatable distortion plugin.",

	.features = (const char *[]) {
		CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
		CLAP_PLUGIN_FEATURE_DISTORTION,
		CLAP_PLUGIN_FEATURE_STEREO,
		NULL,
	},
};

static const clap_plugin_audio_ports_t extensionAudioPorts = {
	.count = [] (const clap_plugin_t *plugin, bool isInput) -> uint32_t { 
		return 1; 
	},

	.get = [] (const clap_plugin_t *plugin, uint32_t index, bool isInput, clap_audio_port_info_t *info) -> bool {
		if (index) return false;

		if (isInput){
			info->id = 0;
			info->channel_count = 2;
			info->flags = CLAP_AUDIO_PORT_IS_MAIN;
			info->port_type = CLAP_PORT_STEREO;
			info->in_place_pair = CLAP_INVALID_ID;
			snprintf(info->name, sizeof(info->name), "%s", "Audio Input");
		}
		else{
			info->id = 0;
			info->channel_count = 2;
			info->flags = CLAP_AUDIO_PORT_IS_MAIN;
			info->port_type = CLAP_PORT_STEREO;
			info->in_place_pair = CLAP_INVALID_ID;
			snprintf(info->name, sizeof(info->name), "%s", "Audio Output");
		}
		return true;
	},
};

static const clap_plugin_params_t extensionParams = {
	.count = [] (const clap_plugin_t *plugin) -> uint32_t {
		return 0;
	},

	.get_info = [] (const clap_plugin_t *_plugin, uint32_t index, clap_param_info_t *information) -> bool {
		return false;
	},

	.get_value = [] (const clap_plugin_t *_plugin, clap_id id, double *value) -> bool {
		return false;
	},

	.value_to_text = [] (const clap_plugin_t *_plugin, clap_id id, double value, char *display, uint32_t size) {
		return false;
	},

	.text_to_value = [] (const clap_plugin_t *_plugin, clap_id param_id, const char *display, double *value) {
		return false;
	},

	.flush = [] (const clap_plugin_t *_plugin, const clap_input_events_t *in, const clap_output_events_t *out) {
		UDShaper *plugin = (UDShaper *) _plugin->plugin_data;
		const uint32_t eventCount = in->size(in);
		// PluginSyncMainToAudio(plugin, out);
	},
};

static const clap_plugin_gui_t extensionGUI = {
	.is_api_supported = [] (const clap_plugin_t *plugin, const char *api, bool isFloating) -> bool {
		return 0 == strcmp(api, GUI_API) && !isFloating;
	},

	.get_preferred_api = [] (const clap_plugin_t *plugin, const char **api, bool *isFloating) -> bool {
		*api = GUI_API;
		*isFloating = false;
		return true;
	},

	.create = [] (const clap_plugin_t *_plugin, const char *api, bool isFloating) -> bool {
		if (!extensionGUI.is_api_supported(_plugin, api, isFloating)) return false;
		GUICreate((UDShaper *) _plugin->plugin_data, pluginDescriptor);
		return true;
	},

	.destroy = [] (const clap_plugin_t *_plugin) {
		GUIDestroy((UDShaper *) _plugin->plugin_data, pluginDescriptor);
	},

	.set_scale = [] (const clap_plugin_t *plugin, double scale) -> bool {
		return false;
	},

	.get_size = [] (const clap_plugin_t *plugin, uint32_t *width, uint32_t *height) -> bool {
		*width = GUI_WIDTH;
		*height = GUI_HEIGHT;
		return true;
	},

	.can_resize = [] (const clap_plugin_t *plugin) -> bool {
		return false;
	},

	.get_resize_hints = [] (const clap_plugin_t *plugin, clap_gui_resize_hints_t *hints) -> bool {
		return false;
	},

	.adjust_size = [] (const clap_plugin_t *plugin, uint32_t *width, uint32_t *height) -> bool {
		return extensionGUI.get_size(plugin, width, height);
	},

	.set_size = [] (const clap_plugin_t *plugin, uint32_t width, uint32_t height) -> bool {
		return true;
	},

	.set_parent = [] (const clap_plugin_t *_plugin, const clap_window_t *window) -> bool {
		assert(0 == strcmp(window->api, GUI_API));
		GUISetParent((UDShaper *) _plugin->plugin_data, window);
		return true;
	},

	.set_transient = [] (const clap_plugin_t *plugin, const clap_window_t *window) -> bool {
		return false;
	},

	.suggest_title = [] (const clap_plugin_t *plugin, const char *title) {
	},

	.show = [] (const clap_plugin_t *_plugin) -> bool {
		UDShaper *plugin = (UDShaper *) _plugin->plugin_data;
		plugin->envelopes->setupForRerender();
		GUISetVisible((UDShaper *) _plugin->plugin_data, true);
		return true;
	},

	.hide = [] (const clap_plugin_t *_plugin) -> bool {
		GUISetVisible((UDShaper *) _plugin->plugin_data, false);
		return true;
	},
};

static const clap_plugin_posix_fd_support_t extensionPOSIXFDSupport = {
	.on_fd = [] (const clap_plugin_t *_plugin, int fd, clap_posix_fd_flags_t flags) {
		UDShaper *plugin = (UDShaper *) _plugin->plugin_data;
		GUIOnPOSIXFD(plugin);
	},
};

static const clap_plugin_timer_support_t extensionTimerSupport = {
	.on_timer = [] (const clap_plugin_t *_plugin, clap_id timerID) {
		UDShaper *plugin = (UDShaper *) _plugin->plugin_data;

		// repaint plugin so Modulation is also visible when user is not giving input
		GUIPaint(plugin, true);
	},
};

static const clap_plugin_t pluginClass = {
	.desc = &pluginDescriptor,
	.plugin_data = nullptr,

	.init = [] (const clap_plugin *_plugin) -> bool {
		UDShaper *plugin = (UDShaper *) _plugin->plugin_data;

		plugin->hostPOSIXFDSupport = (const clap_host_posix_fd_support_t *) plugin->host->get_extension(plugin->host, CLAP_EXT_POSIX_FD_SUPPORT);
		plugin->hostTimerSupport = (const clap_host_timer_support_t *) plugin->host->get_extension(plugin->host, CLAP_EXT_TIMER_SUPPORT);
		plugin->hostParams = (const clap_host_params_t *) plugin->host->get_extension(plugin->host, CLAP_EXT_PARAMS);
		plugin->hostLog = (const clap_host_log_t *) plugin->host->get_extension(plugin->host, CLAP_EXT_LOG);

		if (plugin->hostTimerSupport && plugin->hostTimerSupport->register_timer) {
			plugin->hostTimerSupport->register_timer(plugin->host, GUI_REFRESH_INTERVAL, &plugin->timerID);
		}

		return true;
	},

	.destroy = [] (const clap_plugin *_plugin) {
		UDShaper *plugin = (UDShaper *) _plugin->plugin_data;

		if (plugin->hostTimerSupport && plugin->hostTimerSupport->register_timer) {
			plugin->hostTimerSupport->unregister_timer(plugin->host, plugin->timerID);
		}

		free(plugin);
	},

	.activate = [] (const clap_plugin *_plugin, double sampleRate, uint32_t minimumFramesCount, uint32_t maximumFramesCount) -> bool {
		UDShaper *plugin = (UDShaper *) _plugin->plugin_data;
		return true;
	},

	.deactivate = [] (const clap_plugin *_plugin) {
	},

	.start_processing = [] (const clap_plugin *_plugin) -> bool {
		return true;
	},

	.stop_processing = [] (const clap_plugin *_plugin) {
	},

	.reset = [] (const clap_plugin *_plugin) {
		UDShaper *plugin = (UDShaper *) _plugin->plugin_data;
	},

	.process = [] (const clap_plugin *_plugin, const clap_process_t *process) -> clap_process_status {
		UDShaper *plugin = (UDShaper *) _plugin->plugin_data;

		// Firstly report start and end of the process to the main thread
		reportPlaybackStatusToMain(plugin, process);

		plugin->renderAudio(process);

		return CLAP_PROCESS_CONTINUE;
	},

	.get_extension = [] (const clap_plugin *plugin, const char *id) -> const void * {
		if (0 == strcmp(id, CLAP_EXT_AUDIO_PORTS     )) return &extensionAudioPorts;
		if (0 == strcmp(id, CLAP_EXT_PARAMS          )) return &extensionParams;
		if (0 == strcmp(id, CLAP_EXT_GUI             )) return &extensionGUI;
		if (0 == strcmp(id, CLAP_EXT_POSIX_FD_SUPPORT)) return &extensionPOSIXFDSupport;
		if (0 == strcmp(id, CLAP_EXT_TIMER_SUPPORT   )) return &extensionTimerSupport;
		return nullptr;
	},

	.on_main_thread = [] (const clap_plugin *_plugin) {
	},
};

static const clap_plugin_factory_t pluginFactory = {
	.get_plugin_count = [] (const clap_plugin_factory *factory) -> uint32_t { 
		return 1; 
	},

	.get_plugin_descriptor = [] (const clap_plugin_factory *factory, uint32_t index) -> const clap_plugin_descriptor_t * { 
		return index == 0 ? &pluginDescriptor : nullptr; 
	},

	.create_plugin = [] (const clap_plugin_factory *factory, const clap_host_t *host, const char *pluginID) -> const clap_plugin_t * {
		if (!clap_version_is_compatible(host->clap_version) || strcmp(pluginID, pluginDescriptor.id)) {
			return nullptr;
		}

		// UDShaper *plugin = (UDShaper *) calloc(1, sizeof(UDShaper));
		UDShaper *plugin = new UDShaper(GUI_WIDTH, GUI_HEIGHT);
		plugin->host = host;
		plugin->plugin = pluginClass;
		plugin->plugin.plugin_data = plugin;
		return &plugin->plugin;
	},
};

extern "C" const clap_plugin_entry_t clap_entry = {
	.clap_version = CLAP_VERSION_INIT,

	.init = [] (const char *path) -> bool { 
		return true; 
	},

	.deinit = [] () {},

	.get_factory = [] (const char *factoryID) -> const void * {
		return strcmp(factoryID, CLAP_PLUGIN_FACTORY_ID) ? nullptr : &pluginFactory;
	},
};