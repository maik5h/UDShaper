#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <fstream>
#include <iostream>
#include "clap/clap.h"
#include "config.h"

enum distortionMode {
	upDown,
	midSide,
	leftRight,
	positiveNegative
};

// for debugging
void logToFile(const std::string& message) {
    std::ofstream logFile("C:/Users/mm/Desktop/log.txt", std::ios_base::app);
    if (logFile.is_open()) {
        logFile << message << std::endl;
    } else {
        std::cerr << "Failed to open log file!" << std::endl;
    }
}

// template <class T>
// struct Array {
// 	T *array;
// 	size_t length, allocated;

// 	void Insert(T newItem, uintptr_t index) {
// 		if (length + 1 > allocated) {
// 			allocated *= 2;
// 			if (length + 1 > allocated) allocated = length + 1;
// 			array = (T *) realloc(array, allocated * sizeof(T));
// 		}

// 		length++;
// 		memmove(array + index + 1, array + index, (length - index - 1) * sizeof(T));
// 		array[index] = newItem;
// 	}

// 	void Delete(uintptr_t index) { 
// 		memmove(array + index, array + index + 1, (length - index - 1) * sizeof(T)); 
// 		length--;
// 	}

// 	void Add(T item) { Insert(item, length); }
// 	void Free() { free(array); array = nullptr; length = allocated = 0; }
// 	int Length() { return length; }
// 	T &operator[](uintptr_t index) { assert(index < length); return array[index]; }
// };

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

// TODO make makefile to compile this seperately and include only header.
#include "GUI_utils/shapeEditor.cpp"

struct MyPlugin {
	clap_plugin_t plugin;
	const clap_host_t *host;

	Mutex syncParameters;
	Mutex findInflectionPoints;
	struct GUI *gui;
	const clap_host_posix_fd_support_t *hostPOSIXFDSupport;
	const clap_host_timer_support_t *hostTimerSupport;
	const clap_host_params_t *hostParams;
	bool mouseDragging;
	uint32_t mouseDraggingParameter;
	int32_t mouseDragOriginX, mouseDragOriginY;
	float mouseDragOriginValue;
	clap_id timerID;

	ShapeEditor shapeEditor1;
	ShapeEditor shapeEditor2;

	std::vector<Envelope> envelopes;

	float lastBufferLevelL;
	float lastBufferLevelR;

	distortionMode distortionMode;
};

static void PluginRenderAudio(MyPlugin *plugin, uint32_t start, uint32_t end, float *inputL, float *inputR, float *outputL, float *outputR, double beatPosition, double tempo) {
	switch (plugin->distortionMode) {
		/* TODO Buffer is parallelized into pieces of ~200 samples. I dont know how to access all of them in
		one place so i can not properly decide which shape to choose.
		*/
		case upDown:
		{
			// since data from last buffer is not available, take first samples as a reference, works most of the time but is not optimal.
			bool processShape1L = (inputL[1] >= inputL[0]);
			bool processShape1R = (inputR[1] >= inputR[0]);

			float previousLevelL;
			float previousLevelR;

			for (uint32_t index = start; index < end; index++) {
				// before each modulation step, reset mod values to true default values. Modulations will add up onto this value
				for (Envelope envelope : plugin->envelopes){
					envelope.resetModulatedParameters();
				}
				// for modulation faster than buffer rate, modulate parameters with updated beatPosition for every sample
				for (Envelope envelope : plugin->envelopes){
					// Samplerate is samples per second, so index / samplerate gives time offset in seconds since start of buffer. Tempo, which is beats per minute, multiplied with this value and divided by 60 gives relative beat offset of sample at index.
					// TODO get current samplerate
					int samplerate = 44100;
					double sampleTimeOffset = tempo * index / samplerate / 60;
					// TODO this might be sped up by moving the whole modulation part to the shapeEditor::forward() method, so parameters are only modulated when needed. ForGUI rendering it would still have to be modulated, but at a much slower rate. The two processes could be split up, maybe GUI rendering can be synced to framrate of screen?
					envelope.updateModulatedParameters(beatPosition);
				}

				// update active shape only if value has changed so for plateaus the current shape stays active
				if (index > 0) {
					// if (inputL[index] != previousLevelL) {
						processShape1L = (inputL[index] >= previousLevelL);
					// }
					// if (inputR[index] != previousLevelR) {
						processShape1R = (inputR[index] >= previousLevelR);
					// }
				}

				outputL[index] = processShape1L ? plugin->shapeEditor1.forward(inputL[index]) : plugin->shapeEditor2.forward(inputL[index]);
				outputR[index] = processShape1R ? plugin->shapeEditor1.forward(inputR[index]) : plugin->shapeEditor2.forward(inputR[index]);

				previousLevelL = inputL[index];
				previousLevelR = inputR[index];
			}		
			break;
		}
		case midSide:
		{
			float mid;
			float side;

			for (uint32_t index = start; index < end; index++) {
				mid = plugin->shapeEditor1.forward((inputL[index] + inputR[index])/2);
				side = plugin->shapeEditor2.forward((inputL[index] - inputR[index])/2);

				outputL[index] = mid + side;
				outputR[index] = mid - side;
			}
			break;
		}
		case leftRight:
		{
			for (uint32_t index = start; index < end; index++) {
				outputL[index] = plugin->shapeEditor1.forward(inputL[index]);
				outputR[index] = plugin->shapeEditor2.forward(inputR[index]);
			}
			break;
		}
		case positiveNegative:
		{
			for (uint32_t index = start; index < end; index++) {
				outputL[index] = (inputL[index] > 0) ? plugin->shapeEditor1.forward(inputL[index]) : plugin->shapeEditor2.forward(inputL[index]);
				outputR[index] = (inputR[index] > 0) ? plugin->shapeEditor1.forward(inputR[index]) : plugin->shapeEditor2.forward(inputR[index]);
			}
			break;
		}
	}
}

static void PluginPaintRectangle(MyPlugin *plugin, uint32_t *bits, uint32_t l, uint32_t r, uint32_t t, uint32_t b, uint32_t border, uint32_t fill) {
	for (uint32_t i = t; i < b; i++) {
		for (uint32_t j = l; j < r; j++) {
			bits[i * GUI_WIDTH + j] = (i == t || i == b - 1 || j == l || j == r - 1) ? border : fill;
		}
	}
}

static void PluginPaint(MyPlugin *plugin, uint32_t *bits) {
	PluginPaintRectangle(plugin, bits, 0, GUI_WIDTH, 0, GUI_HEIGHT, 0xC0C0C0, 0xC0C0C0);
	plugin->shapeEditor1.drawGraph(bits);
	plugin->shapeEditor2.drawGraph(bits);
	for (Envelope envelope : plugin->envelopes){
		envelope.drawGraph(bits);
	}
}

static void PluginProcessMouseDrag(MyPlugin *plugin, int32_t x, int32_t y) {
	if (plugin->mouseDragging) {
		// float newValue = FloatClamp01(plugin->mouseDragOriginValue + (plugin->mouseDragOriginY - y) * 0.01f);
		// MutexAcquire(plugin->syncParameters);
		// plugin->mainParameters[plugin->mouseDraggingParameter] = newValue;
		// plugin->mainChanged[plugin->mouseDraggingParameter] = true;
		// MutexRelease(plugin->syncParameters);

		if (plugin->hostParams && plugin->hostParams->request_flush) {
			plugin->hostParams->request_flush(plugin->host);
		}

		plugin->shapeEditor1.processMouseDrag(x, y);
		plugin->shapeEditor2.processMouseDrag(x, y);
		Envelope *envelope = &plugin->envelopes.front();
		while (envelope <= &plugin->envelopes.back()){
			envelope->processMouseDrag(x, y);
			envelope++;
		}
	}
}

static void PluginProcessMousePress(MyPlugin *plugin, int32_t x, int32_t y) {
	// if (x >= 10 && x < 40 && y >= 10 && y < 40) {
	// 	plugin->mouseDragging = true;
	// 	plugin->mouseDraggingParameter = P_VOLUME;
	// 	plugin->mouseDragOriginX = x;
	// 	plugin->mouseDragOriginY = y;
	// 	plugin->mouseDragOriginValue = plugin->mainParameters[P_VOLUME];

	// 	MutexAcquire(plugin->syncParameters);
	// 	plugin->gestureStart[plugin->mouseDraggingParameter] = true;
	// 	MutexRelease(plugin->syncParameters);

	// 	if (plugin->hostParams && plugin->hostParams->request_flush) {
	// 		plugin->hostParams->request_flush(plugin->host);
	// 	}
	// }

	plugin->shapeEditor1.processMousePress(x, y);
	plugin->shapeEditor2.processMousePress(x, y);
	Envelope *envelope = &plugin->envelopes.front();
	while (envelope <= &plugin->envelopes.back()){
		envelope->processMousePress(x, y);
		envelope++;
	}
	plugin->mouseDragging = true;
}

static void PluginProcessMouseRelease(MyPlugin *plugin) {
	if (plugin->mouseDragging) {
		MutexAcquire(plugin->syncParameters);
		// plugin->gestureEnd[plugin->mouseDraggingParameter] = true;
		MutexRelease(plugin->syncParameters);

		if (plugin->hostParams && plugin->hostParams->request_flush) {
			plugin->hostParams->request_flush(plugin->host);
		}

		plugin->shapeEditor1.processMouseRelease();
		plugin->shapeEditor2.processMouseRelease();
		Envelope *envelope = &plugin->envelopes.front();
		while (envelope <= &plugin->envelopes.back()){
			envelope->processMouseRelease();
			envelope++;
		}
		plugin->mouseDragging = false;
	}
}

void PluginProcessDoubleClick(MyPlugin *plugin, uint32_t x, uint32_t y){
	// TODO i feel like this is very very much not elegant
	plugin->shapeEditor1.processDoubleClick(x, y);
	plugin->shapeEditor2.processDoubleClick(x, y);
	Envelope *envelope = &plugin->envelopes.front();
	while (envelope <= &plugin->envelopes.back()){
		envelope->processDoubleClick(x, y);
		envelope++;
	}
}

static const clap_plugin_descriptor_t pluginDescriptor = {
	.clap_version = CLAP_VERSION_INIT,
	.id = "F.UDShaper",
	.name = "UDShaper",
	.vendor = "F",
	.url = "",
	.manual_url = "",
	.support_url = "",
	.version = "1.0.0",
	.description = "Upwards-downwards distortion plugin.",

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
		MyPlugin *plugin = (MyPlugin *) _plugin->plugin_data;
		const uint32_t eventCount = in->size(in);
		// PluginSyncMainToAudio(plugin, out);
	},
};

static const clap_plugin_state_t extensionState = {
	.save = [] (const clap_plugin_t *_plugin, const clap_ostream_t *stream) -> bool {
		MyPlugin *plugin = (MyPlugin *) _plugin->plugin_data;
		// PluginSyncAudioToMain(plugin);
		// return sizeof(float) * P_COUNT == stream->write(stream, plugin->mainParameters, sizeof(float) * P_COUNT);
	},

	.load = [] (const clap_plugin_t *_plugin, const clap_istream_t *stream) -> bool {
		MyPlugin *plugin = (MyPlugin *) _plugin->plugin_data;
		MutexAcquire(plugin->syncParameters);
		// bool success = sizeof(float) * P_COUNT == stream->read(stream, plugin->mainParameters, sizeof(float) * P_COUNT);
		// bool success = sizeof(float) * plugin->mainParameters.size() == stream->read(stream, plugin->mainParameters.data(), sizeof(float) * plugin->mainParameters.size());
		// for (uint32_t i = 0; i < P_COUNT; i++) plugin->mainChanged[i] = true;
		// MutexRelease(plugin->syncParameters);
		// return success;
	},
};

#if defined(_WIN32)
#include "gui_w32.cpp"
#elif defined(__linux__)
#include "gui_x11.cpp"
#elif defined(__APPLE__)
#include "gui_mac.cpp"
#endif

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
		GUICreate((MyPlugin *) _plugin->plugin_data);
		return true;
	},

	.destroy = [] (const clap_plugin_t *_plugin) {
		GUIDestroy((MyPlugin *) _plugin->plugin_data);
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
		GUISetParent((MyPlugin *) _plugin->plugin_data, window);
		return true;
	},

	.set_transient = [] (const clap_plugin_t *plugin, const clap_window_t *window) -> bool {
		return false;
	},

	.suggest_title = [] (const clap_plugin_t *plugin, const char *title) {
	},

	.show = [] (const clap_plugin_t *_plugin) -> bool {
		GUISetVisible((MyPlugin *) _plugin->plugin_data, true);
		return true;
	},

	.hide = [] (const clap_plugin_t *_plugin) -> bool {
		GUISetVisible((MyPlugin *) _plugin->plugin_data, false);
		return true;
	},
};

static const clap_plugin_posix_fd_support_t extensionPOSIXFDSupport = {
	.on_fd = [] (const clap_plugin_t *_plugin, int fd, clap_posix_fd_flags_t flags) {
		MyPlugin *plugin = (MyPlugin *) _plugin->plugin_data;
		GUIOnPOSIXFD(plugin);
	},
};

static const clap_plugin_timer_support_t extensionTimerSupport = {
	.on_timer = [] (const clap_plugin_t *_plugin, clap_id timerID) {
		MyPlugin *plugin = (MyPlugin *) _plugin->plugin_data;

		// repaint plugin so Modulation is also visible when user is not giving input
		GUIPaint(plugin, true);

	},
};

static const clap_plugin_t pluginClass = {
	.desc = &pluginDescriptor,
	.plugin_data = nullptr,

	.init = [] (const clap_plugin *_plugin) -> bool {
		MyPlugin *plugin = (MyPlugin *) _plugin->plugin_data;

		plugin->hostPOSIXFDSupport = (const clap_host_posix_fd_support_t *) plugin->host->get_extension(plugin->host, CLAP_EXT_POSIX_FD_SUPPORT);
		plugin->hostTimerSupport = (const clap_host_timer_support_t *) plugin->host->get_extension(plugin->host, CLAP_EXT_TIMER_SUPPORT);
		plugin->hostParams = (const clap_host_params_t *) plugin->host->get_extension(plugin->host, CLAP_EXT_PARAMS);
		
		uint16_t editorSize1[4] = {50, 50, 450, 650};
		uint16_t editorSize2[4] = {500, 50, 900, 650};
		uint16_t envelopeSize[4] = {950, 50, 1750, 350};
		plugin->shapeEditor1 = ShapeEditor(editorSize1);
		plugin->shapeEditor2 = ShapeEditor(editorSize2);
		plugin->lastBufferLevelL = 0;
		plugin->lastBufferLevelR = 0;
		plugin->distortionMode = upDown;
		plugin->envelopes = {Envelope(envelopeSize)};
		// plugin->envelopes.at(0).addControlledParameter(plugin->shapeEditor1.shapePoints->next, 0, 1, modCurveCenterY);

		MutexInitialise(plugin->syncParameters);

		if (plugin->hostTimerSupport && plugin->hostTimerSupport->register_timer) {
			plugin->hostTimerSupport->register_timer(plugin->host, GUI_REFRESH_INTERVAL, &plugin->timerID);
		}

		return true;
	},

	.destroy = [] (const clap_plugin *_plugin) {
		MyPlugin *plugin = (MyPlugin *) _plugin->plugin_data;
		MutexDestroy(plugin->syncParameters);

		if (plugin->hostTimerSupport && plugin->hostTimerSupport->register_timer) {
			plugin->hostTimerSupport->unregister_timer(plugin->host, plugin->timerID);
		}

		free(plugin);
	},

	.activate = [] (const clap_plugin *_plugin, double sampleRate, uint32_t minimumFramesCount, uint32_t maximumFramesCount) -> bool {
		MyPlugin *plugin = (MyPlugin *) _plugin->plugin_data;
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
		MyPlugin *plugin = (MyPlugin *) _plugin->plugin_data;
	},

	.process = [] (const clap_plugin *_plugin, const clap_process_t *process) -> clap_process_status {
		MyPlugin *plugin = (MyPlugin *) _plugin->plugin_data;

		assert(process->audio_outputs_count == 1);
		assert(process->audio_inputs_count == 1);

		const uint32_t frameCount = process->frames_count; // what is frame count, is it highest sample with event?
		const uint32_t inputEventCount = process->in_events->size(process->in_events);

		const clap_event_transport_t *transport = process->transport;
		clap_transport_flags flag = CLAP_TRANSPORT_IS_PLAYING;
		double beatPosition = (double)transport->song_pos_beats / CLAP_BEATTIME_FACTOR;
		
		// if host is currently playing, render audio
		if (transport->flags[&flag]){

			PluginRenderAudio(plugin, 0, frameCount, process->audio_inputs[0].data32[0], process->audio_inputs[0].data32[1], process->audio_outputs[0].data32[0], process->audio_outputs[0].data32[1], beatPosition, transport->tempo);
		
		}
		
		// if not, reset all modulations so values snap back to the true unmodulated values
		else{
			for (Envelope envelope : plugin->envelopes){
				envelope.resetModulatedParameters();
			}
		}

		return CLAP_PROCESS_CONTINUE;
	},

	.get_extension = [] (const clap_plugin *plugin, const char *id) -> const void * {
		if (0 == strcmp(id, CLAP_EXT_AUDIO_PORTS     )) return &extensionAudioPorts;
		if (0 == strcmp(id, CLAP_EXT_PARAMS          )) return &extensionParams;
		if (0 == strcmp(id, CLAP_EXT_GUI             )) return &extensionGUI;
		if (0 == strcmp(id, CLAP_EXT_POSIX_FD_SUPPORT)) return &extensionPOSIXFDSupport;
		if (0 == strcmp(id, CLAP_EXT_TIMER_SUPPORT   )) return &extensionTimerSupport;
		if (0 == strcmp(id, CLAP_EXT_STATE           )) return &extensionState;
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

		MyPlugin *plugin = (MyPlugin *) calloc(1, sizeof(MyPlugin));
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