#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <fstream>
#include <iostream>
#include <vector>
#include <chrono>
#include "clap/clap.h"
#include "config.h"
#include "GUI_utils/shapeEditor.h"
#include "GUI_utils/assets.h"

// Returns the current time since Unix epoch in milliseconds.
long getCurrentTime(){
	auto time = std::chrono::system_clock::now();
	auto since_epoch = time.time_since_epoch();
	auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(since_epoch);
	return millis.count();
}

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

struct MyPlugin {
	clap_plugin_t plugin;
	const clap_host_t *host;

	Mutex syncParameters;
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

	EnvelopeManager envelopes;

	distortionMode distortionMode;

	// I could not come up waith a more elegant solution yet, to get current beat position outside of audio thread, save time at playback was started and initial beat position.
	long startedPlaying;
	long stoppedPlaying;
	double initBeatPosition;
	Mutex setPlayingStartTime;

	double currentTempo;
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

			// TODO get current samplerate
			int samplerate = 44100;

			for (uint32_t index = start; index < end; index++) {
				// for modulation faster than buffer rate, modulate parameters with updated beatPosition for every sample
				// tempo is beats per minute, so tempo / 60 is beats per second. samplerate is samples per second, so tempo/60 * samplerate is beats per sample.
				beatPosition += tempo / samplerate / 60;

				// update active shape only if value has changed so for plateaus the current shape stays active
				if (index > 0) {
					// if (inputL[index] != previousLevelL) {
						processShape1L = (inputL[index] >= previousLevelL);
					// }
					// if (inputR[index] != previousLevelR) {
						processShape1R = (inputR[index] >= previousLevelR);
					// }
				}

				outputL[index] = processShape1L ? plugin->shapeEditor1.forward(inputL[index], beatPosition) : plugin->shapeEditor2.forward(inputL[index], beatPosition);
				outputR[index] = processShape1R ? plugin->shapeEditor1.forward(inputR[index], beatPosition) : plugin->shapeEditor2.forward(inputR[index], beatPosition);

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
				mid = plugin->shapeEditor1.forward((inputL[index] + inputR[index])/2, beatPosition);
				side = plugin->shapeEditor2.forward((inputL[index] - inputR[index])/2, beatPosition);

				outputL[index] = mid + side;
				outputR[index] = mid - side;
			}
			break;
		}
		case leftRight:
		{
			for (uint32_t index = start; index < end; index++) {
				outputL[index] = plugin->shapeEditor1.forward(inputL[index], beatPosition);
				outputR[index] = plugin->shapeEditor2.forward(inputR[index], beatPosition);
			}
			break;
		}
		case positiveNegative:
		{
			for (uint32_t index = start; index < end; index++) {
				outputL[index] = (inputL[index] > 0) ? plugin->shapeEditor1.forward(inputL[index], beatPosition) : plugin->shapeEditor2.forward(inputL[index], beatPosition);
				outputR[index] = (inputR[index] > 0) ? plugin->shapeEditor1.forward(inputR[index], beatPosition) : plugin->shapeEditor2.forward(inputR[index], beatPosition);
			}
			break;
		}
	}
}

static void PluginPaint(MyPlugin *plugin, uint32_t *canvas) {

	// TODO how to get beatPosition on main thread???
	double beatPosition = 0;

	plugin->shapeEditor1.drawGraph(canvas, beatPosition);
	plugin->shapeEditor2.drawGraph(canvas, beatPosition);
	plugin->envelopes.renderGUI(canvas);
}

static void PluginProcessMouseDrag(MyPlugin *plugin, uint32_t x, uint32_t y) {
	if (plugin->mouseDragging) {
		if (plugin->hostParams && plugin->hostParams->request_flush) {
			plugin->hostParams->request_flush(plugin->host);
		}

		plugin->shapeEditor1.processMouseDrag(x, y);
		plugin->shapeEditor2.processMouseDrag(x, y);

		plugin->envelopes.processMouseDrag(x, y);
	}
}

static void PluginProcessMousePress(MyPlugin *plugin, int32_t x, int32_t y) {

	plugin->shapeEditor1.processMouseClick(x, y);
	plugin->shapeEditor2.processMouseClick(x, y);

	plugin->envelopes.processMouseClick(x, y);
	plugin->mouseDragging = true;
}

static contextMenuType PluginProcessMouseRelease(MyPlugin *plugin) {
	contextMenuType menu = menuNone; // The menu to show after processing the mouse release. Is menuPointPosMod if a new modulation link was added to a point, menuNone else.
	if (plugin->mouseDragging) {

		// TODO i dont know what this does
		if (plugin->hostParams && plugin->hostParams->request_flush) {
			plugin->hostParams->request_flush(plugin->host);
		}

		// If it was attempted to link the active Envelope to a modulatable parameter, check if a parameter was selected successfully and add a ModulatedParameter instance to the corresponding Envelope.
		if (plugin->envelopes.currentDraggingMode == addLink){
			// Get the closest points for both ShapeEditors to the position the mouse was dragged to
			ShapePoint *closestPoint1, *closestPoint2;

			closestPoint1 = plugin->shapeEditor1.getClosestPoint(plugin->envelopes.draggedToX, plugin->envelopes.draggedToY);
			closestPoint2 = plugin->shapeEditor2.getClosestPoint(plugin->envelopes.draggedToX, plugin->envelopes.draggedToY);

			// check if a close point was found
			if ((closestPoint1 != nullptr) | (closestPoint2 != nullptr)){
				ShapePoint *closest = (closestPoint1 == nullptr) ? closestPoint2 : closestPoint1;
				// Since getClosestPoint() was called on the ShapeEditor instances, their currentDraggingMode value has been set to either position or curveCenter and can be used to find the correct parameter to modulate.
				shapeEditorDraggingMode mode = (closestPoint1 == nullptr) ? plugin->shapeEditor2.currentDraggingMode : plugin->shapeEditor1.currentDraggingMode;
				
				// Find the correct parameter to modulate. There are the options curveCenter, posX and posY.
				modulationMode modMode;
				if (mode == curveCenter) {
					modMode = modCurveCenterY;
				}
				// If the point position should be modulated, show context menu to select correct direction.
				// Exception: last point is selected, which can only be modulated in y-direction. Do not show the menu in this case and directly add the ModulatedParameter.
				else {
					menu = menuPointPosMod;
				}

				// Add a ModulatedParameter to the active Envelope.
				plugin->envelopes.attemptedToModulate = closest;
			}
		}
		plugin->envelopes.processMouseRelease();

		plugin->shapeEditor1.processMouseRelease();
		plugin->shapeEditor2.processMouseRelease();

		plugin->mouseDragging = false;
	}
	return menu;
}

void PluginProcessDoubleClick(MyPlugin *plugin, uint32_t x, uint32_t y){
	// If a point has been deleted by the double click, a pointer will be returned by ShapeEditor::processDoubleClick.
	ShapePoint *deletedPoint1 = plugin->shapeEditor1.processDoubleClick(x, y);
	ShapePoint *deletedPoint2 = plugin->shapeEditor2.processDoubleClick(x, y);

	// Clear all modulation links to the deleted points to avoid dangling pointers.
	plugin->envelopes.clearLinksToPoint(deletedPoint1);
	plugin->envelopes.clearLinksToPoint(deletedPoint2);

	plugin->envelopes.processDoubleClick(x, y);
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
		
		uint32_t editorSize1[4] = {50, 50, 450, 650};
		uint32_t editorSize2[4] = {495, 50, 895, 650};
		uint32_t envelopeSize[4] = {950, 50, 1900, 500};
		plugin->shapeEditor1 = ShapeEditor(editorSize1);
		plugin->shapeEditor2 = ShapeEditor(editorSize2);
		plugin->distortionMode = upDown;
		plugin->envelopes = EnvelopeManager(envelopeSize);

		MutexInitialise(plugin->syncParameters);
		MutexInitialise(plugin->setPlayingStartTime);

		if (plugin->hostTimerSupport && plugin->hostTimerSupport->register_timer) {
			plugin->hostTimerSupport->register_timer(plugin->host, GUI_REFRESH_INTERVAL, &plugin->timerID);
		}

		return true;
	},

	.destroy = [] (const clap_plugin *_plugin) {
		MyPlugin *plugin = (MyPlugin *) _plugin->plugin_data;
		MutexDestroy(plugin->syncParameters);
		MutexDestroy(plugin->setPlayingStartTime);

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
		clap_transport_flags isNotPlaying = CLAP_TRANSPORT_IS_PLAYING;
		double beatPosition = (double)transport->song_pos_beats / CLAP_BEATTIME_FACTOR;

		// tempo is needed to calculate the exact beat position for every sample of the buffer. However, if the host is not playing, a tempo of zero is passed so all parameters stay constant.
		double tempo = transport->flags[&isNotPlaying] ? transport->tempo : 0;

		PluginRenderAudio(plugin, 0, frameCount, process->audio_inputs[0].data32[0], process->audio_inputs[0].data32[1], process->audio_outputs[0].data32[0], process->audio_outputs[0].data32[1], beatPosition, tempo);

		plugin->currentTempo = transport->tempo;

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