#define PLUG_NAME "UDShaper"
#define PLUG_MFR "maik5h"
#define PLUG_VERSION_HEX 0x00000000
#define PLUG_VERSION_STR "0.0.0"
#define PLUG_UNIQUE_ID 'udsp'
#define PLUG_MFR_ID 'mk5h'
#define PLUG_URL_STR "https://iplug2.github.io"
#define PLUG_EMAIL_STR ""
#define PLUG_COPYRIGHT_STR ""
#define PLUG_CLASS_NAME UDShaper

#define BUNDLE_NAME "UDShaper"
#define BUNDLE_MFR "maik5h"
#define BUNDLE_DOMAIN "com"

#define SHARED_RESOURCES_SUBPATH "UDShaper"

#define PLUG_CHANNEL_IO "1-1 2-2"

#define PLUG_LATENCY 0
#define PLUG_TYPE 0
#define PLUG_DOES_MIDI_IN 0
#define PLUG_DOES_MIDI_OUT 0
#define PLUG_DOES_MPE 0
#define PLUG_DOES_STATE_CHUNKS 0
#define PLUG_HAS_UI 1
#define PLUG_WIDTH 1000
#define PLUG_HEIGHT 500
#define PLUG_FPS 60
#define PLUG_SHARED_RESOURCES 0
#define PLUG_HOST_RESIZE 0

#define AUV2_ENTRY UDShaper_Entry
#define AUV2_ENTRY_STR "UDShaper_Entry"
#define AUV2_FACTORY UDShaper_Factory
#define AUV2_VIEW_CLASS UDShaper_View
#define AUV2_VIEW_CLASS_STR "UDShaper_View"

#define AAX_TYPE_IDS 'IEF1', 'IEF2'
#define AAX_TYPE_IDS_AUDIOSUITE 'IEA1', 'IEA2'
#define AAX_PLUG_MFR_STR "Acme"
#define AAX_PLUG_NAME_STR "UDShaper\nIPEF"
#define AAX_PLUG_CATEGORY_STR "Effect"
#define AAX_DOES_AUDIOSUITE 1

#define VST3_SUBCATEGORY "Fx"

#define CLAP_MANUAL_URL "https://github.com/maik5h/UDShaper"
#define CLAP_SUPPORT_URL ""
#define CLAP_DESCRIPTION "Fully modulatable upwards-downwards distortion plugin."
#define CLAP_FEATURES "audio-effect", "distortion", "stereo"

#define APP_NUM_CHANNELS 2
#define APP_N_VECTOR_WAIT 0
#define APP_MULT 1
#define APP_COPY_AUV3 0
#define APP_SIGNAL_VECTOR_SIZE 64

#define ROBOTO_FN "Roboto-Regular.ttf"
