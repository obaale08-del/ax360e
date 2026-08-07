// SPDX-License-Identifier: WTFPL
#include "emulator_ax360e.h"
#include "ax360e_emu.h"

#include "xenia/app/emulator_window.h"
#include "xenia/emulator.h"
#include "xenia/apu/nop/nop_audio_system.h"
#include "xenia/gpu/null/null_graphics_system.h"
#include "xenia/hid/nop/nop_hid.h"
#include "xenia/base/logging.h"
#include "xenia/vfs/devices/stfs_xbox.h"
#include "xenia/base/mapped_memory.h"

#include "xenia/kernel/util/xex2_info.h"      // xex2_header, xex2_opt_execution_info, XEX_HEADER_EXECUTION_INFO
#include "xenia/cpu/xex_module.h"             // XexModule::GetOptHeader, kXEX2Signature/kXEX1Signature

#include "cpuinfo.h"
#include "vkapi.h"
#include "vkutil.h"

//#include "cpptoml/include/cpptoml.h"

#include <fcntl.h>
#include <unistd.h>
#include <algorithm>

#include <android/log.h>
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "ax360e", __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, "ax360e", __VA_ARGS__)

jclass g_class_DocumentFile;
jclass g_class_Emulator;

jobject g_context;
jobject g_doocument_file_tree;

jmethodID mid_open_uri_fd;

std::vector<std::string> g_launch_args;
std::string g_native_lib_dir;

extern JavaVM* g_jvm;
static void j_setup_context(JNIEnv* env,jobject self,jobject context ){
    g_context = env->NewGlobalRef(context);
    //getApplicationInfo().nativeLibraryDir;
    jmethodID mid_get_application_info = env->GetMethodID(env->GetObjectClass(context), "getApplicationInfo", "()Landroid/content/pm/ApplicationInfo;");
    jobject app_info = env->CallObjectMethod(context, mid_get_application_info);
    jfieldID mid_native_library_dir = env->GetFieldID(env->GetObjectClass(app_info), "nativeLibraryDir", "Ljava/lang/String;");
    jstring native_library_dir = (jstring)env->GetObjectField(app_info, mid_native_library_dir);
    const char* native_library_dir_c_str=env->GetStringUTFChars(native_library_dir,NULL);
    g_native_lib_dir=native_library_dir_c_str;
    env->ReleaseStringUTFChars(native_library_dir,native_library_dir_c_str);
}

//public native void setup_document_file_tree(DocumentFile tree);
static void j_setup_document_file_tree(JNIEnv* env,jobject self,jobject tree ){
    g_doocument_file_tree = env->NewGlobalRef(tree);
}

//public native void setup_launch_args(String[] args);
static void j_setup_launch_args(JNIEnv* env,jobject self,jobjectArray args ){
    g_launch_args.clear();
    for(int i=0;i<env->GetArrayLength(args);i++){
        jstring arg=(jstring)env->GetObjectArrayElement(args,i);
        g_launch_args.push_back(env->GetStringUTFChars(arg,NULL) );
    }
}


static jstring j_simple_device_info(JNIEnv* env, jobject thiz)
{
    std::string info;

    auto get_gpu_info=[]()->std::string {
        std::pair<std::string,bool> lib_info={"libvulkan.so",false};
        vk_load(lib_info.first.c_str(),lib_info.second);

        struct clean_t{
            std::vector<std::function<void()>> funcs;
            ~clean_t(){
                for(auto it=funcs.rbegin();it!=funcs.rend();it++){
                    (*it)();
                }
            }
        }clean;

        clean.funcs.push_back([](){
            vk_unload();
        });

        std::optional<VkInstance> inst=vk_create_instance("ax360e-gpu_info");
        if(!inst) {
            return "获取gpu信息失败";
        }

        clean.funcs.push_back([=](){
            vk_destroy_instance(*inst);
        });

        if(int count=vk_get_physical_device_count(*inst);count!=1) {

            if(count<1){
                return "获取gpu信息失败";
            }
            if(count>1){
                return "多个gpu!";
            }
        }
        if(auto pdev=vk_get_physical_device(*inst);pdev) {
            std::string gpu_name=vk_get_physical_device_properties(*pdev).deviceName;
            std::string gpu_vk_ver=[](uint32_t v) {
                std::ostringstream oss;
                oss << (v >> 22) << "." << ((v >> 12) & 0x3ff) << "." << (v & 0xfff);
                return oss.str();
            }(vk_get_physical_device_properties(*pdev).apiVersion);

            std::string gpu_ext=[&]() {
                std::ostringstream oss;
                for (auto ext : vk_get_physical_device_extension_properties(*pdev)) {
                    oss <<"    * " << ext.extensionName << "\n";
                }
                return oss.str();
            }();
            return "GPU [" + gpu_name +"(Vulkan: "+gpu_vk_ver+ ")]:\n" + gpu_ext;

        }
        return "获取gpu信息失败";
    };

    auto get_cpu_info=[]()->std::string {

        std::vector<core_info_t> core_info=cpu_get_core_info();
        std::string cpu_name=cpu_get_simple_info(core_info);
        std::string cpu_features=[&](){
            std::ostringstream oss;
            for(const auto& feature : core_info[0].features){
                oss <<"    * " << feature << "\n";
            }
            return oss.str();
        }();
        return "CPU [" + cpu_name + "]:\n" + cpu_features;
    };

    info+=get_cpu_info();
    info+="\n"+get_gpu_info();

    return env->NewStringUTF(info.c_str());
}

static jobject j_meta_info_from_god_game(JNIEnv* env,jobject self,jobject context,jstring uri_str ) {
    jclass cls_Emulator$GameInfo = env->FindClass("aenu/ax360e/Emulator$GameInfo");
    jmethodID mid_Emulator$GameInfo = env->GetMethodID(cls_Emulator$GameInfo, "<init>", "()V");
    jfieldID fid_name = env->GetFieldID(cls_Emulator$GameInfo, "name", "Ljava/lang/String;");
    jfieldID fid_uri = env->GetFieldID(cls_Emulator$GameInfo, "uri", "Ljava/lang/String;");
    jfieldID fid_icon = env->GetFieldID(cls_Emulator$GameInfo, "icon", "[B");
    jfieldID fid_title_id = env->GetFieldID(cls_Emulator$GameInfo, "title_id", "Ljava/lang/String;");

    jclass uri_class = env->FindClass("android/net/Uri");
    jmethodID parse_method = env->GetStaticMethodID(uri_class, "parse", "(Ljava/lang/String;)Landroid/net/Uri;");

    jobject game_info = env->NewObject(cls_Emulator$GameInfo, mid_Emulator$GameInfo);
    env->SetObjectField(game_info, fid_uri, uri_str);

    jobject uri = env->CallStaticObjectMethod(uri_class, parse_method, uri_str);

    xe::vfs::XContentContainerHeader header;
    // read header
    {
        //public static int nc_open_uri_fd(Context ctx,Uri uri)
        int header_file_fd = env->CallStaticIntMethod(g_class_Emulator, mid_open_uri_fd, context, uri);

        if (header_file_fd == -1) {
            return NULL;
        }
        std::unique_ptr<xe::MappedMemory> mmap = xe::MappedMemory::OpenForUnixFd(header_file_fd);
        if (!mmap) {
            return NULL;
        }
        if(mmap->size() < sizeof(header)) {
            return NULL;
        }
        std::memcpy(&header, mmap->data(), sizeof(header));
    }

    std::string name = xe::to_utf8(header.content_metadata.title_name());
    env->SetObjectField(game_info, fid_name, env->NewStringUTF(name.c_str()));

    // Set title_id
    {
        uint32_t tid = static_cast<uint32_t>(header.content_metadata.execution_info.title_id);
        char tid_buf[9];
        snprintf(tid_buf, sizeof(tid_buf), "%08X", tid);
        env->SetObjectField(game_info, fid_title_id, env->NewStringUTF(tid_buf));
    }

    jbyteArray icon = env->NewByteArray(header.content_metadata.thumbnail_size);
    env->SetByteArrayRegion(icon, 0, header.content_metadata.thumbnail_size, (const jbyte*)header.content_metadata.thumbnail);
    env->SetObjectField(game_info, fid_icon, icon);
    return game_info;
}
#if 0
static std::unique_ptr<xe::apu::AudioSystem> create_nop_audio_system(
        xe::cpu::Processor* processor) {
    return std::make_unique<xe::apu::nop::NopAudioSystem>(processor);
}

static std::unique_ptr<xe::gpu::GraphicsSystem> create_null_graphics_system() {
    return std::make_unique<xe::gpu::null::NullGraphicsSystem>();
}

static std::vector<std::unique_ptr<xe::hid::InputDriver>> create_nop_input_drivers(
        xe::ui::Window* window) {

    std::vector<std::unique_ptr<xe::hid::InputDriver>> drivers;
    drivers.emplace_back(xe::hid::nop::Create(window, xe::app::EmulatorWindow::kZOrderHidInput));

    return drivers;
}
//public native GameInfo meta_info_from_uri(String uri) throws RuntimeException;
static jobject j_meta_info_from_uri(JNIEnv* env,jobject self,jstring uri_str ){

    /*
    public static class GameInfo{
        public String name;
        public String uri;
        public int fd;
        public byte[] icon;
     */
    jclass cls_Emulator$GameInfo = env->FindClass("aenu/ax360e/Emulator$GameInfo");
    jmethodID mid_Emulator$GameInfo = env->GetMethodID(cls_Emulator$GameInfo, "<init>", "()V");
    jobject game_info = env->NewObject(cls_Emulator$GameInfo, mid_Emulator$GameInfo);
    jfieldID fid_name = env->GetFieldID(cls_Emulator$GameInfo, "name", "Ljava/lang/String;");
    jfieldID fid_uri = env->GetFieldID(cls_Emulator$GameInfo, "uri", "Ljava/lang/String;");
    env->SetObjectField(game_info, fid_uri, uri_str);


    jclass uri_class = env->FindClass("android/net/Uri");
    jmethodID parse_method = env->GetStaticMethodID(uri_class, "parse", "(Ljava/lang/String;)Landroid/net/Uri;");
    jobject uri = env->CallStaticObjectMethod(uri_class, parse_method, uri_str);

    std::unique_ptr<DocumentFile> file = DocumentFile::find(env, uri);

    std::vector<char*> args;
    args.push_back(NULL);
    for(auto& i:g_launch_args){
        args.push_back((char*)i.c_str());
    }

    int argc=args.size();
    char** argv=args.data();

    cvar::ParseLaunchArguments(argc, argv, "",{});
    xe::InitializeLogging(file->getName());

    AndroidWindowedAppContext app_context;
    std::unique_ptr<xe::Emulator> emulator = std::make_unique<xe::Emulator>("","","","");
    auto emulator_wnd = xe::app::EmulatorWindow::Create(emulator.get(), app_context);
    xe::X_STATUS result = emulator->Setup(
            emulator_wnd->window(), emulator_wnd->imgui_drawer(), true,
            create_nop_audio_system, create_null_graphics_system, create_nop_input_drivers);
    if (XFAILED(result)) {
        env->SetObjectField(game_info, fid_name, env->NewStringUTF("???")) ;
        return game_info;
    }
    std::string result_str;
    bool ret=false;
    emulator->on_launch.AddListener([&](auto title_id, const auto& game_title) {
        result_str=game_title.empty() ? "Unknown Title" : std::string(game_title);
        XELOGI("#############: {}", result_str);
        ret=true;
    });

    std::string name = file->getName();
    if(name.ends_with(".xex")){
        result = emulator->LaunchXexFile(std::move(file));
    }
    else{
        const char* path = env->GetStringUTFChars(uri_str,NULL);
        std::string data_dir = std::string (path)+".data";
        env->ReleaseStringUTFChars(uri_str,path);

        jstring data_dir_str = env->NewStringUTF(data_dir.c_str());
        jobject data_dir_uri = env->CallStaticObjectMethod(uri_class, parse_method, data_dir_str);

        std::unique_ptr<DocumentFile> data_dir_file =
                DocumentFile::find(env, data_dir_uri);

        result = emulator->LaunchStfsContainer(std::move(file), std::move(data_dir_file));
    }

    if (XFAILED(result)) {
        env->SetObjectField(game_info, fid_name, env->NewStringUTF("????")) ;
        return game_info;
    }

    while (!ret);
    XELOGI("################Game: {}", result_str);
    env->SetObjectField(game_info, fid_name, env->NewStringUTF(result_str.c_str())) ;
    return game_info;
}
#endif

static const std::string gen_skips[]={
        //"CPU",
        "Config",
        "a64",
        "Profiles",
        "Vulkan|vulkan_device",

        "Storage|cache_root",
        "Storage|content_root",
        "Storage|storage_root",
        "Kernel|kernel_display_gamma_power",
        "Kernel|cl",
        "Kernel|kernel_build_version",
        "Kernel|default_achievements_backend",

        "Display|postprocess_ffx_cas_additional_sharpness",
        "Display|present_safe_area_y",
        "Display|postprocess_ffx_fsr_max_upsampling_passes",
        "Display|present_safe_area_x",
        "Display|postprocess_ffx_fsr_sharpness_reduction",


        "GPU|dump_shaders",
        "GPU|draw_resolution_scale_x",
        "GPU|primitive_processor_cache_min_indices",
        "GPU|query_occlusion_fake_sample_count",
        "GPU|texture_cache_memory_limit_soft_lifetime",
        "GPU|draw_resolution_scale_y",
        "GPU|trace_gpu_prefix",
        "GPU|texture_cache_memory_limit_render_to_texture",

        "GPU|query_occlusion_sample_lower_threshold",
        "GPU|query_occlusion_sample_upper_threshold",
        "GPU|framerate_limit",

        "CPU|pvr",
        "CPU|load_module_map",
         "CPU|break_condition_op",
         "CPU|trace_function_data",
         "CPU|trace_function_data_path",
          "CPU|break_condition_value",
           "CPU|break_on_instruction",
            "CPU|break_condition_gpr",

        "Logging|log_file",
        "Logging|log_mask",

        "Video|internal_display_resolution_x",
        "Video|internal_display_resolution_y",

        "HID|left_stick_deadzone_percentage",
        "HID|right_stick_deadzone_percentage",
        "HID|vibration",

        "XConfig|audio_flag",

        "General|notification_sound_path",
        "General|launch_module",


};
using entries=std::vector<std::string>;
static const std::pair<std::string,entries> gen_list[]={
        //str
        {"APU|apu",{"nop","aaudio","opensles"}},
        {"Display|postprocess_antialiasing",{"none", "fxaa", "fxaa_extreme"}},
        {"Display|postprocess_scaling_and_sharpening",{"bilinear", "cas", "fsr"}},
        {"GPU|gpu",{"vulkan", "null"}},
        {"GPU|render_target_path_vulkan",{"any", "fbo","fsi"}},
        {"HID|hid",{"android", "nop"}},
        {"CPU|cpu",{"any","a64"}},

        //int
        {"Content|license_mask",{"disable@0","first@1","all@-1"}},
        {"XConfig|user_country",{"AE@1","AL@2", "AM@3", "AR@4", "AT@5", "AU@6", "AZ@7", "BE@8", "BG@9"
                                 , "BH@10", "BN@11", "BO@12", "BR@13", "BY@14", "BZ@15", "CA@16", "CH@18", "CL@19"
                                 , "CN@20", "CO@21", "CR@22", "CZ@23", "DE@24", "DK@25", "DO@26", "DZ@27", "EC@28"
                                 , "EE@29", "EG@30", "ES@31", "FI@32", "FO@33", "FR@34", "GB@35", "GE@36", "GR@37"
                                 , "GT@38", "HK@39", "HN@40", "HR@41", "HU@42", "ID@43", "IE@44", "IL@45", "IN@46"
                                 , "IQ@47", "IR@48", "IS@49", "IT@50", "JM@51", "JO@52", "JP@53", "KE@54", "KG@55"
                                 , "KR@56", "KW@57", "KZ@58", "LB@59", "LI@60", "LT@61", "LU@62", "LV@63", "LY@64"
                                 , "MA@65", "MC@66", "MK@67", "MN@68", "MO@69", "MV@70", "MX@71", "MY@72", "NI@73"
                                 , "NL@74", "NO@75", "NZ@76", "OM@77", "PA@78", "PE@79", "PH@80", "PK@81", "PL@82"
                                 , "PR@83", "PT@84", "PY@85", "QA@86", "RO@87", "RU@88", "SA@89", "SE@90", "SG@91"
                                 , "SI@92", "SK@93", "SV@95", "SY@96", "TH@97", "TN@98", "TR@99", "TT@100","TW@101"
                                 , "UA@102", "US@103", "UY@104", "UZ@105", "VE@106", "VN@107", "YE@108", "ZA@109"
                                 }},
        {"XConfig|user_language",{"en@1","ja@2","de@3","fr@4","es@5","it@6","ko@7","zh@8"
                                 ,"pt@9","pl@11","ru@12","sv@13","tr@14","nb@15","nl@16","zh@17"}},
        {"Vulkan|vulkan_debug_utils_messenger_severity",{"error@0","warning@1","info@2","verbose@3"}},

        {"Kernel|kernel_display_gamma_type",{"linear@0","sRGB(CRT)@1","BT.709(HDTV)@2",/*kernel_display_gamma_power@3*/}},
        {"Logging|log_level",{"error@0","warning@1","info@2","debug@3",}},
        /*
                                                  	#  0 = PAL-60 Component (SD)
                                                  	#  1 = Unused
                                                  	#  2 = PAL-60 SCART
                                                  	#  3 = 480p Component (HD)
                                                  	#  4 = HDMI+A
                                                  	#  5 = PAL-60 Composite/S-Video
                                                  	#  6 = VGA
                                                  	#  7 = TV PAL-60
                                                  	#  8 = HDMI (default)*/
        {"Video|avpack",{"PAL-60 Component (SD)@0", "Unused@1","PAL-60 SCART@2","480p Component (HD)@3","HDMI+A@4","PAL-60 Composite/S-Video@5","VGA@6","TV PAL-60@7","HDMI@8"}},
        /*#    1=NTSC
                                                  	#    2=NTSC-J
                                                  	#    3=PAL*/
        {"Video|video_standard",{ "NTSC@1","NTSC-J@2","PAL-60@3"}},
/*#    0=640x480
                                                  	#    1=640x576
                                                  	#    2=720x480
                                                  	#    3=720x576
                                                  	#    4=800x600
                                                  	#    5=848x480
                                                  	#    6=1024x768
                                                  	#    7=1152x864
                                                  	#    8=1280x720 (Default)
                                                  	#    9=1280x768
                                                  	#    10=1280x960
                                                  	#    11=1280x1024
                                                  	#    12=1360x768
                                                  	#    13=1440x900
                                                  	#    14=1680x1050
                                                  	#    15=1920x540
                                                  	#    16=1920x1080*/
        {"Video|internal_display_resolution",{ "640x480@0","640x576@1","720x480@2","720x576@3","800x600@4","848x480@5","1024x768@6","1152x864@7","1280x720@8"
                                               ,"1280x768@9","1280x960@10","1280x1024@11","1360x768@12","1440x900@13", "1680x1050@14","1920x540@15","1920x1080@16"}},
                                               /*Kernel = 1, Apu = 2, Cpu = 4.*/
        //{"Logging|log_file"}
        {"APU|xma_decoder",{"fake", "master", "old", "new"}},
        {"GPU|readback_resolve",{"fast","full","none"}},

};

using range=std::pair<int,int>;
static const std::pair<std::string,range> gen_seekbar[]={
        {"GPU|texture_cache_memory_limit_hard",{512,4096}},
        {"GPU|texture_cache_memory_limit_soft",{512,4096}},
        {"Memory|mmap_address_high",{2,63}},
        {"APU|apu_max_queued_frames",{4,64}},
        {"APU|xmp_default_volume",{0,100}},
        {"General|time_scalar",{1,8}},
        //{"Video|internal_display_resolution_x",{1,1920}},
        //{"Video|internal_display_resolution_y",{1,1080}},
};

#define SEEKBAR_PREF_TAG "aenu.preference.SeekBarPreference"
#define CHECKBOX_PREF_TAG "aenu.preference.CheckBoxPreference"
#define LIST_PREF_TAG "aenu.preference.ListPreference"
#if 0

static jstring generate_config_xml(JNIEnv* env,jobject self,jstring toml_path){
    return env->NewStringUTF("out.str().c_str()");
}
#else
static jstring generate_config_xml(JNIEnv* env,jobject self,jstring toml_path){

    jboolean is_copy=false;
    const char* path=env->GetStringUTFChars(toml_path,&is_copy);

    toml::table* toml = new toml::table(toml::parse_file(path));
    env->ReleaseStringUTFChars(toml_path,path);

    std::ostringstream out;
    out<<R"(
<?xml version="1.0" encoding="utf-8"?>
<PreferenceScreen
    xmlns:android="http://schemas.android.com/apk/res/android"
    xmlns:app="http://schemas.android.com/apk/res-auto">
    )";

    for(auto table_iter=toml->begin() ;table_iter!=toml->end();table_iter++){
        const std::string table_name(table_iter->first);
        if(std::find(std::begin(gen_skips),std::end(gen_skips),table_name)!=std::end(gen_skips))
            continue;
        std::string table_name_l(table_name); std::transform(table_name_l.begin(),table_name_l.end(),table_name_l.begin(),::tolower);
        toml::table* table=table_iter->second.as_table();
        out<<"<PreferenceScreen app:title=\"@string/es_"<<table_name_l<<"\" \n";
        out<<"app:iconSpaceReserved=\"false\" \n";
        out<<"app:key=\""<<table_name<<"\" >\n";

        for(auto iter=table->begin();iter!=table->end();iter++){
            const std::string key_name(iter->first);
            const std::string find_key=table_name+"|"+key_name;
            if(std::find(std::begin(gen_skips),std::end(gen_skips),find_key)!=std::end(gen_skips))
                continue;

            {
                auto find_iter=std::begin(gen_list);
                find_iter=std::find_if(find_iter,std::end(gen_list),[&find_key](const std::pair<std::string,entries>& entry){
                    return entry.first==find_key;
                });
                if(find_iter!=std::end(gen_list)){
                    out<<"<" LIST_PREF_TAG " app:title=\"@string/es_"<<table_name_l<<"_"<<key_name<<"\" \n";
                    if(std::find(std::begin(find_iter->second[0]),std::end(find_iter->second[0]),'@')!=std::end(find_iter->second[0])){
                        out<<"app:entryValues=\"@array/es_arr_v_"<<table_name_l<<"_"<<key_name<<"\" \n";
                    }
                    else{
                        out<<"app:entryValues=\"@array/es_arr_"<<table_name_l<<"_"<<key_name<<"\" \n";
                    }
                    out<<"app:entries=\"@array/es_arr_"<<table_name_l<<"_"<<key_name<<"\" \n";
                    out<<"app:iconSpaceReserved=\"false\" \n";
                    out<<"app:key=\""<<table_name<<"|"<<key_name<<"\" />\n";
                    continue;
                }
            }

            {
                auto find_iter=std::begin(gen_seekbar);
                find_iter=std::find_if(find_iter,std::end(gen_seekbar),[&find_key](const std::pair<std::string,range>& entry){
                    return entry.first==find_key;
                });
                if(find_iter!=std::end(gen_seekbar)){
                    out<<"<" SEEKBAR_PREF_TAG " app:title=\"@string/es_"<<table_name_l<<"_"<<key_name<<"\" \n";
                    out<<"app:min=\""<<find_iter->second.first<<"\"\n";
                    out<<"android:max=\""<<find_iter->second.second<<"\"\n";
                    out<<"app:showSeekBarValue=\"true\"\n";
                    out<<"app:iconSpaceReserved=\"false\" \n";
                    out<<"app:key=\""<<table_name<<"|"<<key_name<<"\" />\n";
                    continue;
                }
            }

            if(const auto val=table->get_as<bool>(key_name);val){
                std::string val_str=*val?"true":"false";
                out<<"<" CHECKBOX_PREF_TAG " app:title=\"@string/es_"<<table_name_l<<"_"<<key_name<<"\" \n";
                out<<"app:iconSpaceReserved=\"false\" \n";
                out<<"app:key=\""<<table_name<<"|"<<key_name<<"\" />\n";
            }
            /*else if(const auto val=table->get_as<int>(key_name);val){
                out<<"<" SEEKBAR_PREF_TAG " app:title=\"@string/es_"<<table_name_l<<"_"<<key_name<<"\" \n";
                out<<"app:showSeekBarValue=\"true\"\n";
                out<<"app:key=\""<<table_name<<"|"<<key_name<<"\" />\n";
            }*/
            else if(const auto val=table->get_as<double>(key_name);val){
                //FIXME
                out<<"<PreferenceScreen app:title=\"@string/es_"<<table_name_l<<"_"<<key_name<<"\" \n";
                out<<"app:iconSpaceReserved=\"false\" \n";
                out<<"app:key=\""<<table_name<<"|"<<key_name<<"\" />\n";
            }
            else if(const auto val=table->get_as<std::string>(key_name);val){
                //FIXME
                out<<"<PreferenceScreen app:title=\"@string/es_"<<table_name_l<<"_"<<key_name<<"\" \n";
                out<<"app:iconSpaceReserved=\"false\" \n";
                out<<"app:key=\""<<table_name<<"|"<<key_name<<"\" />\n";
            }
        }

        out<<"</PreferenceScreen>\n";
    }

    out<<"</PreferenceScreen>\n";

    //JAVA const String[]
    out<<"\n\n\n\n";

    out<<"final String[] BOOL_KEYS={\n";
    for(auto table_iter=toml->begin() ;table_iter!=toml->end();table_iter++){
        const std::string table_name(table_iter->first);
        toml::table* table=table_iter->second.as_table();
        if(std::find(std::begin(gen_skips),std::end(gen_skips),table_name)!=std::end(gen_skips))
            continue;
        for(auto iter=table->begin();iter!=table->end();iter++){
            const std::string key_name(iter->first);
            const std::string find_key=table_name+"|"+key_name;
            if(std::find(std::begin(gen_skips),std::end(gen_skips),find_key)!=std::end(gen_skips))
                continue;
                if(const auto val=table->get_as<bool>(key_name);val){
                    out<<"\""<<table_name<<"|"<<key_name<<"\",\n";
                }
        }
    }
    out<<"};\n";

    out<<"final String[] INT_KEYS={\n";
    for(auto table_iter=toml->begin() ;table_iter!=toml->end();table_iter++){
        const std::string table_name(table_iter->first);
        toml::table* table=table_iter->second.as_table();
        if(std::find(std::begin(gen_skips),std::end(gen_skips),table_name)!=std::end(gen_skips))
            continue;
        for(auto iter=table->begin();iter!=table->end();iter++){
            const std::string key_name(iter->first);
            const std::string find_key=table_name+"|"+key_name;
            if(std::find(std::begin(gen_skips),std::end(gen_skips),find_key)!=std::end(gen_skips))
                continue;
            {
                auto find_iter=std::begin(gen_seekbar);
                find_iter=std::find_if(find_iter,std::end(gen_seekbar),[&find_key](const std::pair<std::string,range>& entry){
                    return entry.first==find_key;
                });
                if(find_iter!=std::end(gen_seekbar)){
                    out<<"\""<<table_name<<"|"<<key_name<<"\",\n";
                }
            }

        }
    }
    out<<"};\n";
    out<<"final String[] STRING_ARR_KEYS={\n";
    for(auto table_iter=toml->begin() ;table_iter!=toml->end();table_iter++){
        const std::string table_name(table_iter->first);
        toml::table* table=table_iter->second.as_table();
        if(std::find(std::begin(gen_skips),std::end(gen_skips),table_name)!=std::end(gen_skips))
            continue;
            for(auto iter=table->begin();iter!=table->end();iter++){
                const std::string key_name(iter->first);
                const std::string find_key=table_name+"|"+key_name;
                if(std::find(std::begin(gen_skips),std::end(gen_skips),find_key)!=std::end(gen_skips))
                    continue;
                {
                    auto find_iter=std::begin(gen_list);
                    find_iter=std::find_if(find_iter,std::end(gen_list),[&find_key](const std::pair<std::string,entries>& entry){
                        return entry.first==find_key;
                    });
                    if(find_iter!=std::end(gen_list)){
                        out<<"\""<<table_name<<"|"<<key_name<<"\",\n";
                    }
                }
            }
    }
    out<<"};\n";

#if 1
    //STRING XML
    out<<"\n\n\n\n";

    auto convert_to_name=[](const std::string& key){
        std::string result=key;
        replace(result.begin(),result.end(),'_',' ');
        result[0]=toupper(result[0]);
        for(int i=1;i<result.size();i++){
            if(result[i]==' '){
                if(i+1<result.size())
                    result[i+1]=toupper(result[i+1]);
            }
        }
        return result;
    };

    for(auto table_iter=toml->begin() ;table_iter!=toml->end();table_iter++){
        const std::string table_name(table_iter->first);
        std::string table_name_l=table_name;
        std::transform(table_name_l.begin(),table_name_l.end(),table_name_l.begin(),::tolower);
        out<<"<string name=\"es_"<<table_name_l<<"\">"<<table_name<<"</string>\n";

        toml::table* table=table_iter->second.as_table();
        for(auto iter=table->begin();iter!=table->end();iter++){
            const std::string key_name(iter->first);
            out<<"<string name=\"es_"<<table_name_l<<"_"<<key_name<<"\">"<<convert_to_name(key_name)<<"</string>\n";
        }
    }

#endif
#if 1
    //STRING ARRAY XML
    out<<"\n\n\n\n";

    for(auto table_iter=toml->begin() ;table_iter!=toml->end();table_iter++){
        const std::string table_name(table_iter->first);
        std::string table_name_l=table_name;
        std::transform(table_name_l.begin(),table_name_l.end(),table_name_l.begin(),::tolower);

        toml::table* table=table_iter->second.as_table();
        for(auto iter=table->begin();iter!=table->end();iter++){
            const std::string key_name(iter->first);
            auto find_iter=std::begin(gen_list);
            find_iter=std::find_if(find_iter,std::end(gen_list),[&table_name,&key_name](const std::pair<std::string,entries>& entry){
                return entry.first==table_name+"|"+key_name;
            });
            if(find_iter!=std::end(gen_list)){
                auto list=find_iter->second;
                if(std::find(std::begin(list[0]),std::end(list[0]),'@')!=std::end(list[0])){
                    out<<"<string-array name=\"es_arr_v_"<<table_name_l<<"_"<<key_name<<"\">\n";
                    for(auto entry:list){
                        int pos=entry.find("@");
                        std::string entry_str=entry.substr(pos+1);
                        out<<"<item>"<<entry_str<<"</item>\n";
                    }
                    out<<"</string-array>\n";
                    out<<"<string-array name=\"es_arr_"<<table_name_l<<"_"<<key_name<<"\">\n";
                    for(auto entry:list){
                        int pos=entry.find("@");
                        std::string entry_str=entry.substr(0,pos);
                        out<<"<item>"<<entry_str<<"</item>\n";
                    }
                    out<<"</string-array>\n";
                }
                else{
                    out<<"<string-array name=\"es_arr_"<<table_name_l<<"_"<<key_name<<"\">\n";
                    for(auto entry:list){
                        out<<"<item>"<<entry<<"</item>\n";
                    }
                    out<<"</string-array>\n";
                }
            }
        }
    }
#endif
    return env->NewStringUTF(out.str().c_str());
}
#endif
#undef SEEKBAR_PREF_TAG
#undef CHECKBOX_PREF_TAG
#undef LIST_PREF_TAG

//https://github.com/rfandango/XenDroid/blob/main/emulator-core/src/main/cpp/emulator_xendroid.cpp
// Combined name+icon (+title_id) result for the single-decompress scan path.
struct XexMeta {
    std::string name;            // "" if absent / unreadable / failed charset validation
    std::vector<uint8_t> icon;   // empty if absent / unreadable
    uint32_t title_id = 0;       // 0 if unreadable
    uint32_t media_id = 0;       // 0 if unreadable
    uint32_t disc_number = 0;    // 1-based; 0 if not stated
    uint32_t disc_count = 0;     // 0 if not stated
};

// Lightweight, allocation-free structural validation that accepts only well-formed,
// BMP-only (1-3 byte) UTF-8 -- the subset JNI NewStringUTF can consume. Rejects
// embedded NUL, overlong-2, lone continuations, and ALL 4-byte (supplementary-plane)
// sequences. We do NOT use xe::to_utf8 here: that overload only takes
// std::u16string_view (xenia/base/string.h) and is for the UTF-16 GOD title; the SPA
// title is already a raw std::string of (Xbox-convention) UTF-8/ASCII bytes.
static bool is_well_formed_utf8(const std::string& s) {
    const auto* p = reinterpret_cast<const unsigned char*>(s.data());
    const size_t n = s.size();
    for (size_t i = 0; i < n;) {
        const unsigned char c = p[i];
        if (c == 0x00) return false;              // embedded NUL -> reject
        size_t extra;
        if (c < 0x80) { extra = 0; }
        else if ((c & 0xE0) == 0xC0) { if (c < 0xC2) return false; extra = 1; }  // reject overlong 2-byte
        else if ((c & 0xF0) == 0xE0) { extra = 2; }
            // Reject ALL 4-byte leads: JNI NewStringUTF consumes MODIFIED UTF-8, which has
            // no 4-byte form (supplementary chars are CESU-8 surrogate pairs). Game titles
            // are effectively always BMP, so a 4-byte char -> reject -> filename fallback
            // rather than feed NewStringUTF a form it cannot represent.
        else { return false; }                    // 4-byte (0xF0-0xF7), lone cont. (0x80-0xBF), 0xF8-0xFF
        if (i + extra >= n) return false;          // truncated multibyte
        for (size_t k = 1; k <= extra; k++) {
            if ((p[i + k] & 0xC0) != 0x80) return false;  // bad continuation
        }
        i += extra + 1;
    }
    return true;
}
// BOUNDED title-name read for ONE language from an already-parsed XDBF/SPA. Does NOT
// call spa.Load(): SpaInfo::LoadLanguageData (spa_info.cc:50-83) walks the XSTR string
// table with `ptr += string_length + 4` and NO clamp to section->data.size(), so a
// crafted SPA can OOB-read -> host SIGSEGV the caller's try/catch cannot intercept.
// Here we walk the SAME section ourselves, clamping every read to the bounded
// section->data vector (a real memcpy'd copy of size info.size, xdbf_io.h:156-160), so
// the worst crafted input yields "" instead of a crash.
//
// Section keying mirrors LoadLanguageData/title_name: the kStringTable (0x0003) section
// is keyed by the LANGUAGE NUMBER; the per-string id is matched on
// XdbfStringTableEntry.id == kXdbfIdTitle (0x8000). GetEntry(uint16,uint64) const is
// PUBLIC (xdbf_io.h:175) so no Load()/const_cast is needed.
static std::string read_spa_title_for_language(
        const xe::kernel::xam::SpaInfo& spa, xe::XLanguage lang) {
    using namespace xe::kernel::xam;

    const Entry* section = spa.GetEntry(
            static_cast<uint16_t>(SpaSection::kStringTable),
            static_cast<uint64_t>(lang));
    if (!section) return "";

    const uint8_t* const begin = section->data.data();
    const size_t avail = section->data.size();
    if (avail < sizeof(XdbfSectionHeaderEx)) return "";

    const auto* header = reinterpret_cast<const XdbfSectionHeaderEx*>(begin);
    if (header->magic != kXdbfSignatureXstr) return "";  // not a string table

    const uint8_t* ptr = begin + sizeof(XdbfSectionHeaderEx);
    const uint8_t* const end = begin + avail;
    const uint16_t count = header->count;

    for (uint16_t i = 0; i < count; i++) {
        // Need the 4-byte entry header.
        if (ptr + sizeof(XdbfStringTableEntry) > end) return "";
        const auto* entry = reinterpret_cast<const XdbfStringTableEntry*>(ptr);
        const uint16_t str_len = entry->string_length;
        const uint8_t* str_ptr = ptr + sizeof(XdbfStringTableEntry);
        // Need str_len payload bytes (no wrap: str_ptr <= end already, str_len<=0xFFFF).
        if (str_ptr + str_len > end) return "";

        if (entry->id == static_cast<uint16_t>(kXdbfIdTitle)) {
            std::string result(reinterpret_cast<const char*>(str_ptr), str_len);
            // Cut at first embedded NUL (Modified-UTF-8/NewStringUTF mis-handles it).
            const size_t nul = result.find('\0');
            if (nul != std::string::npos) result.resize(nul);
            if (result.empty() || !is_well_formed_utf8(result)) return "";
            return result;  // valid UTF-8 title
        }
        ptr = str_ptr + str_len;
    }
    return "";  // title id not present in this language's table
}
// PREFER the ENGLISH title so a game authored for another region still shows its
// English name in the library; fall back to the game's default language only when an
// English string table is absent (e.g. a Japan-only release), so we still show
// *something* rather than the filename.
static std::string read_spa_title_name_bounded(
        const xe::kernel::xam::SpaInfo& spa) {
    std::string name = read_spa_title_for_language(spa, xe::XLanguage::kEnglish);
    if (!name.empty()) return name;

    const xe::XLanguage def = spa.default_language();  // public; ctor-parsed, no Load()
    if (def != xe::XLanguage::kEnglish) {
        name = read_spa_title_for_language(spa, def);
    }
    return name;
}

// Light, boot-free XEX2 header walk -> execution_info.title_id.
// data/size span at least the first header_size bytes of the .xex (the whole
// mmap is fine). Returns false (and leaves *out untouched) on any malformed /
// missing-header case. Mirrors XexModule::GetOptHeader offset semantics for the
// EXECUTION_INFO (0x00040006, low-byte 0x06 -> offset) optional header.
// NOTE: the xex2_* structs live in namespace xe (xex2_info.h), while XexModule /
// kXEX*Signature live in xe::cpu (xex_module.h).
static bool read_xex_title_id(const uint8_t* data, size_t size, uint32_t* out,
                              uint32_t* media_id_out = nullptr,
                              uint32_t* disc_number_out = nullptr,
                              uint32_t* disc_count_out = nullptr) {
    using namespace xe;            // xex2_header, xex2_opt_*, XEX_HEADER_EXECUTION_INFO
    using namespace xe::cpu;       // XexModule, kXEX2Signature, kXEX1Signature

    if (!data || size < sizeof(xex2_header)) return false;
    auto* h = reinterpret_cast<const xex2_header*>(data);
    const uint32_t magic = h->magic.get();
    if (magic != kXEX2Signature && magic != kXEX1Signature) return false;

    // Bound the optional-header directory inside the buffer (0x18 fixed header,
    // then header_count entries of sizeof(xex2_opt_header) each).
    const uint32_t count = h->header_count.get();
    if (0x18ull + uint64_t(count) * sizeof(xex2_opt_header) > size) return false;

    // Use the non-templated void** overload directly: the templated form casts
    // away const on the out pointer, which the compiler rejects. GetOptHeader only
    // does pointer arithmetic off `h`, never writes through it.
    void* exec_raw = nullptr;
    if (!XexModule::GetOptHeader(h, XEX_HEADER_EXECUTION_INFO, &exec_raw))
        return false;              // title lacks execution_info
    if (!exec_raw) return false;

    // GetOptHeader's default branch computes header + offset with no bounds check
    // on offset, so this guard is load-bearing for untrusted files.
    const auto* p = reinterpret_cast<const uint8_t*>(exec_raw);
    if (p < data || p + sizeof(xex2_opt_execution_info) > data + size) return false;

    auto* exec_info = reinterpret_cast<const xex2_opt_execution_info*>(p);
    *out = exec_info->title_id.get();
    if (media_id_out) *media_id_out = exec_info->media_id.get();
    // Raw uint8_t, 1-based, 0 = not stated.
    if (disc_number_out) *disc_number_out = exec_info->disc_number;
    if (disc_count_out) *disc_count_out = exec_info->disc_count;
    return true;
}
// Decompress default.xex into a transient guest address space and pull BOTH the
// XDBF/SPA title NAME and title icon PNG out of its title-id resource section, plus
// the title id (free, already computed). Returns a XexMeta with any subset of fields
// populated; empty/zero on failure. base/size must span the full .xex image.
// SAFETY: three guards on the decompress path (single-Memory atomic guard,
// resource-header bounds, res-span image-extent clamp). The NAME is read via the
// BOUNDED read_spa_title_name_bounded (no spa.Load(), no OOB-walk).
static XexMeta extract_xex_meta(const uint8_t* base, size_t size) {
    using namespace xe;            // xe::Memory, xex2_* structs, XEX_HEADER_RESOURCE_INFO
    using namespace xe::cpu;       // XexModule, Processor, kXEX*Signature

    XexMeta out;
    if (!base || size < sizeof(xex2_header)) return out;

    // Quick magic pre-check so we don't stand up a 4GB mapping for non-XEX data.
    auto* hdr = reinterpret_cast<const xex2_header*>(base);
    const uint32_t magic = hdr->magic.get();
    if (magic != kXEX2Signature && magic != kXEX1Signature) return out;
    // header_size must be inside the buffer (Load memcpy's header_size bytes).
    if (hdr->header_size.get() > size) return out;

    // xe::Memory owns a PROCESS-GLOBAL singleton (active_memory_) and, on
    // XE_PLATFORM_xendroid, a single FIXED host-base mapping -- so only ONE may be
    // alive per process at a time. The only caller is the library scan, which runs
    // in the main/library process and never boots a game in-process (the emulator
    // runs in the separate :emu process, with its own per-process active_memory_),
    // so it never collides with a live game's Memory. This guard ADDITIONALLY
    // enforces that invariant within this process: if a Memory is already standing
    // (a concurrent/re-entrant scan), bail with no icon rather than letting a second
    // ctor silently corrupt the active_memory_ singleton in release builds.
    static std::atomic<bool> s_memory_in_use{false};
    bool expected = false;
    if (!s_memory_in_use.compare_exchange_strong(expected, true)) return out;
    struct MemoryGuard { ~MemoryGuard() { s_memory_in_use.store(false); } } mem_guard;

    // Transient guest address space + bare processor. RAII frees both on scope
    // exit; ~Memory unmaps the fixed-base views. processor must outlive xex_module
    // (XexModule holds memory_ = processor->memory()); declare in that order so
    // destruction is xex_module -> processor -> memory.
    xe::Memory memory;
    if (!memory.Initialize()) return out;
    xe::cpu::Processor processor(&memory, nullptr /*export_resolver*/);

    // kernel_state may be null: Load()/ReadImage never dereference it (only
    // LoadContinue/import-resolution do, which we deliberately skip).
    auto xex_module = std::make_unique<XexModule>(&processor, nullptr);

    // Wrap in a try/catch: ReadImage hits assert_*/may throw on crafted inputs
    // in debug; in release asserts are off but keep the guard for any std throw.
    bool loaded = false;
    try {
        loaded = xex_module->Load("default", "default.xex", base, size);
    } catch (...) {
        loaded = false;
    }
    if (!loaded) return out;  // bad magic / all keys failed / no valid PE

    // Replicate UserModule::GetSection (user_module.cc:281-302) to find the title's
    // resource section. The resource directory is an optional header that lives
    // UNCOMPRESSED in the XEX header region (the first header_size bytes), so read it
    // straight from the raw file with the SAME load-bearing bounds guard as
    // read_xex_title_id -- GetOptHeader's default branch computes header+offset with
    // NO validation, so a crafted resource offset could otherwise point anywhere.
    void* res_raw = nullptr;
    if (!XexModule::GetOptHeader(hdr, XEX_HEADER_RESOURCE_INFO, &res_raw) || !res_raw)
        return out;  // no resources
    const uint8_t* rp = reinterpret_cast<const uint8_t*>(res_raw);
    if (rp < base || rp + sizeof(xex2_opt_resource_info) > base + size) return out;
    auto* res_hdr = reinterpret_cast<const xex2_opt_resource_info*>(rp);
    const uint32_t res_blob_size = res_hdr->size.get();
    if (res_blob_size < 4) return out;
    const uint32_t count = (res_blob_size - 4) / uint32_t(sizeof(xex2_resource));
    // Bound the full resources[count] array inside the file before iterating it.
    if (uint64_t(rp - base) + 4 + uint64_t(count) * sizeof(xex2_resource) > size)
        return out;

    // The title-id resource is named with the 8-char uppercase-hex title id.
    // Pull the title id straight from the header (reuse read_xex_title_id).
    uint32_t title_id = 0;
    uint32_t media_id = 0;
    uint32_t disc_number = 0, disc_count = 0;
    if (!read_xex_title_id(base, size, &title_id, &media_id, &disc_number,
                           &disc_count) ||
        title_id == 0)
        return out;
    out.title_id = title_id;                                   // capture (free)
    out.media_id = media_id;
    out.disc_number = disc_number;
    out.disc_count = disc_count;
    const std::string res_name = fmt::format("{:08X}", title_id);  // exactly 8 chars

    uint32_t res_addr = 0, res_size = 0;
    for (uint32_t i = 0; i < count; i++) {
        const xex2_resource& r = res_hdr->resources[i];
        if (std::memcmp(r.name, res_name.data(), 8) == 0) {
            res_addr = r.address.get();
            res_size = r.size.get();
            break;
        }
    }
    if (res_addr == 0 || res_size == 0) return out;  // no title resource

    // Validate [res_addr, res_addr+res_size) lies WHOLLY inside the loaded image
    // extent [base_address, base_address+image_size) before handing it to the parser.
    // res_addr/res_size come from the untrusted header, so this keeps the resource
    // span backed by real, decompressed image bytes.
    if (uint64_t(res_addr) + res_size < res_addr) return out;            // wrap
    const uint32_t img_base = xex_module->base_address();
    const uint64_t img_end = uint64_t(img_base) + xex_module->image_size();
    if (res_addr < img_base || uint64_t(res_addr) + res_size > img_end) return out;
    if (!memory.LookupHeap(res_addr)) return out;
    uint8_t* res_ptr = memory.TranslateVirtual(res_addr);
    if (!res_ptr) return out;

    // Parse XDBF/SPA once: icon via ctor-parsed title_icon(); name via the BOUNDED
    // reader (NO spa.Load()). Both copied out BEFORE memory unmaps. The bounded name
    // reader cannot OOB, but title_icon() and the SpaInfo ctor (Entry memcpy, no
    // offset/size clamp -- xdbf_io.h:156-160) carry a residual XDBF-trust risk on a
    // crafted-but-loadable XEX, the same one the upstream module_xdbf path carries;
    // the try/catch below only catches std throws, not a SIGSEGV from that parser.
    try {
        xe::kernel::xam::SpaInfo spa(std::span<uint8_t>(res_ptr, res_size));
        std::span<const uint8_t> icon = spa.title_icon();
        if (!icon.empty()) {
            out.icon.assign(icon.begin(), icon.end());  // copy out BEFORE memory unmaps
        }
        out.name = read_spa_title_name_bounded(spa);  // "" on any failure
    } catch (...) {
        out.icon.clear();
        out.name.clear();
    }
    return out;  // memory/processor/xex_module torn down here
}


// ===========================================================================
// All-Files-Access (real-path) scan natives.
//
// These mount the core's REAL-PATH devices (DiscImageDevice / DiscZarchiveDevice)
// or call the header-only Extract*Metadata(path) helpers from a std::filesystem
// path -- no Context, no ContentResolver, no /proc/self/fd. Used when the user
// has granted MANAGE_EXTERNAL_STORAGE (the only games path).
// format codes match TID_FMT_* / GameFormat.titleIdCode (0=ISO,1=XEX_FOLDER,2=ZAR).
// ===========================================================================

// Read a single VFS entry fully into a buffer via Open/ReadSync (decompresses
// .zar; reads disc extents). Empty on any failure. Shared by the ISO + ZAR
// real-path metadata routes (real-path DiscImageEntry exposes no public
// mmap/offset accessor, unlike the SAF entry, so go through the File API).
static std::vector<uint8_t> read_vfs_entry_bytes(xe::vfs::Entry* e) {
    using namespace xe;
    std::vector<uint8_t> out;
    if (!e) return out;
    vfs::File* in = nullptr;
    if (e->Open(vfs::FileAccess::kFileReadData, &in) != X_STATUS_SUCCESS || !in) {
        return out;
    }
    const size_t size = e->size();
    out.resize(size);
    size_t total = 0;
    while (total < size) {
        size_t got = 0;
        if (in->ReadSync(std::span<uint8_t>(out.data() + total, size - total),
                         total, &got) != X_STATUS_SUCCESS ||
            got == 0) {
            break;
        }
        total += got;
    }
    in->Destroy();
    out.resize(total);  // trim to bytes actually read
    return out;
}

// Read default.xex out of a real-path ISO / .zar into a buffer (empty on
// failure), mounting the core's real-path DiscImageDevice / DiscZarchiveDevice.
static std::vector<uint8_t> read_disc_default_xex(std::unique_ptr<DocumentFile> file) {
    using namespace xe;
    std::vector<uint8_t> out;
    xe::vfs::SAF_DiscImageDevice dev("\\Device\\Cdrom0", std::move(file));
    if (!dev.Initialize()) return out;             // not XDVDFS / corrupt
    out = read_vfs_entry_bytes(dev.ResolvePath("default.xex"));
    return out;  // dev (mmap / ZArchiveReader) freed here
}

static std::vector<uint8_t> read_zar_default_xex(std::unique_ptr<DocumentFile> file) {
    using namespace xe;
    std::vector<uint8_t> out;
    xe::vfs::SAF_DiscZarchiveDevice dev("\\Device\\Cdrom0", std::move(file));
    if (!dev.Initialize()) return out;             // not a valid .zar
    out = read_vfs_entry_bytes(dev.ResolvePath("default.xex"));
    return out;  // dev (mmap / ZArchiveReader) freed here
}
static jobject game_metadata_to_jobject(JNIEnv* env, jstring uri_str,
                                        const XexMeta& meta) {
    jclass cls_GameInfo = env->FindClass("aenu/ax360e/Emulator$GameInfo");
    jmethodID mid_GameInfo = env->GetMethodID(cls_GameInfo, "<init>", "()V");
    jfieldID fid_uri = env->GetFieldID(cls_GameInfo, "uri", "Ljava/lang/String;");
    jfieldID fid_name = env->GetFieldID(cls_GameInfo, "name", "Ljava/lang/String;");
    jfieldID fid_icon = env->GetFieldID(cls_GameInfo, "icon", "[B");
    jfieldID fid_title_id = env->GetFieldID(cls_GameInfo, "title_id", "Ljava/lang/String;");

    jobject game_info = env->NewObject(cls_GameInfo, mid_GameInfo);
    env->SetObjectField(game_info, fid_uri, uri_str);

    if (!meta.name.empty()) {
        env->SetObjectField(game_info, fid_name, env->NewStringUTF(meta.name.c_str()));
    }

    if (!meta.icon.empty()) {
        jbyteArray icon = env->NewByteArray(meta.icon.size());
        env->SetByteArrayRegion(icon, 0, meta.icon.size(), (const jbyte*)meta.icon.data());
        env->SetObjectField(game_info, fid_icon, icon);
    }

    if (meta.title_id) {
        char tid_buf[9];
        snprintf(tid_buf, sizeof(tid_buf), "%08X", meta.title_id);
        env->SetObjectField(game_info, fid_title_id, env->NewStringUTF(tid_buf));
    }

    return game_info;
}

static std::unique_ptr<DocumentFile> from_single_uri(JNIEnv* env, jobject context, jstring uri_str){
    jclass uri_class = env->FindClass("android/net/Uri");
    jmethodID parse_method = env->GetStaticMethodID(uri_class, "parse", "(Ljava/lang/String;)Landroid/net/Uri;");
    jobject uri = env->CallStaticObjectMethod(uri_class, parse_method, uri_str);

    jclass doc_file_class = env->FindClass("androidx/documentfile/provider/DocumentFile");
    jmethodID from_single_uri = env->GetStaticMethodID(doc_file_class, "fromSingleUri",
                                                       "(Landroid/content/Context;Landroid/net/Uri;)Landroidx/documentfile/provider/DocumentFile;");
    jobject doc_file_obj = env->CallStaticObjectMethod(doc_file_class, from_single_uri, context, uri);
    if (!doc_file_obj) return nullptr;
    return std::make_unique<DocumentFile>(g_jvm, doc_file_obj);
}

//public native GameInfo meta_info_from_iso_game(Context ctx,String uri)
static jobject j_meta_info_from_iso_game(JNIEnv* env, jobject self, jobject context, jstring uri_str) {

    g_context = context;

    auto file = from_single_uri(env, context, uri_str);
    if (!file) {
        return nullptr;
    }

    XexMeta meta;  // name/icon empty, title_id 0 on failure
    // read header
    {
        std::vector<uint8_t> xex = read_disc_default_xex(std::move(file));
        if (!xex.empty()) meta = extract_xex_meta(xex.data(), xex.size());
    }

    // Nothing readable at all -> null (Kotlin keeps the filename name, no icon).
    if (meta.name.empty() && meta.icon.empty() && meta.title_id == 0) {
        return nullptr;
    }

    return game_metadata_to_jobject(env, uri_str, meta);

}

// public native GameInfo meta_info_from_zar_game(Context ctx,String uri)
static jobject j_meta_info_from_zar_game(JNIEnv* env, jobject self, jobject context, jstring uri_str) {

    g_context = context;

    auto file = from_single_uri(env, context, uri_str);
    if (!file) {
        return nullptr;
    }

    XexMeta meta;  // name/icon empty, title_id 0 on failure
    // read header
    {
        std::vector<uint8_t> xex = read_zar_default_xex(std::move(file));
        if (!xex.empty()) meta = extract_xex_meta(xex.data(), xex.size());
    }

    // Nothing readable at all -> null (Kotlin keeps the filename name, no icon).
    if (meta.name.empty() && meta.icon.empty() && meta.title_id == 0) {
        return nullptr;
    }

    return game_metadata_to_jobject(env, uri_str, meta);
}

static jobject j_meta_info_from_xex_game(JNIEnv* env, jobject self, jobject context,
                                         jstring boot_xex_uri) {

    jclass uri_class = env->FindClass("android/net/Uri");
    jmethodID parse_method = env->GetStaticMethodID(uri_class, "parse", "(Ljava/lang/String;)Landroid/net/Uri;");

    jobject uri = env->CallStaticObjectMethod(uri_class, parse_method, boot_xex_uri);

    XexMeta meta;  // name/icon empty, title_id 0 on failure
    // read header
    {
        //public static int nc_open_uri_fd(Context ctx,Uri uri)
        int header_file_fd = env->CallStaticIntMethod(g_class_Emulator, mid_open_uri_fd, context, uri);

        if (header_file_fd == -1) {
            return NULL;
        }
        std::unique_ptr<xe::MappedMemory> mmap = xe::MappedMemory::OpenForUnixFd(header_file_fd);
        if (!mmap) {
            return NULL;
        }

        meta = extract_xex_meta(mmap->data(), mmap->size());
    }

    // extract_xex_meta yields the NAME + ICON but its title-id parse may bail
    // before it (e.g. no resources) even when the header carries a valid id.
    // Backfill the title-id from the header-only Extract*Metadata(path) so the
    // per-game config stem is still populated (matches the SAF fallback intent).
    /*if (meta.title_id == 0) {
        std::optional<xe::vfs::XexMetadata> hdr;
        switch (format) {
            case TID_FMT_ISO:        hdr = xe::vfs::ExtractIsoMetadata(p); break;
            case TID_FMT_XEX_FOLDER: hdr = xe::vfs::ExtractXexMetadata(p); break;
            case TID_FMT_ZAR:        hdr = xe::vfs::ExtractZarMetadata(p); break;
            default: break;
        }
        if (hdr) {
            meta.title_id = hdr->title_id;
            meta.media_id = hdr->media_id;
            meta.disc_number = hdr->disc_number;
            meta.disc_count = hdr->disc_count;
        }
    }*/

    // Nothing readable at all -> null (Kotlin keeps the filename name, no icon).
    if (meta.name.empty() && meta.icon.empty() && meta.title_id == 0) {
        return nullptr;
    }

    return game_metadata_to_jobject(env, boot_xex_uri, meta);
}

int register_ax360e_Emulator(JNIEnv* env){

    g_class_DocumentFile=env->FindClass("androidx/documentfile/provider/DocumentFile");
    g_class_DocumentFile=(jclass)env->NewGlobalRef(g_class_DocumentFile);

    g_class_Emulator = env->FindClass("aenu/ax360e/Emulator");
    g_class_Emulator = (jclass)env->NewGlobalRef(g_class_Emulator);

    //public static int nc_open_uri_fd(Context ctx,String uri)
    mid_open_uri_fd = env->GetStaticMethodID(g_class_Emulator, "nc_open_uri_fd", "(Landroid/content/Context;Landroid/net/Uri;)I");

    static const JNINativeMethod methods[] = {
            { "setup_context", "(Landroid/content/Context;)V", (void *) j_setup_context },
            { "setup_document_file_tree", "(Landroidx/documentfile/provider/DocumentFile;)V", (void *) j_setup_document_file_tree },
            { "setup_launch_args", "([Ljava/lang/String;)V", (void *) j_setup_launch_args },
            { "meta_info_from_god_game", "(Landroid/content/Context;Ljava/lang/String;)Laenu/ax360e/Emulator$GameInfo;", (void *) j_meta_info_from_god_game },
            {"simple_device_info", "()Ljava/lang/String;", (void *) j_simple_device_info}
            ,{"generate_config_xml", "(Ljava/lang/String;)Ljava/lang/String;", (void *) generate_config_xml}
            ,{"meta_info_from_iso_game", "(Landroid/content/Context;Ljava/lang/String;)Laenu/ax360e/Emulator$GameInfo;", (void *) j_meta_info_from_iso_game}
            ,{"meta_info_from_zar_game", "(Landroid/content/Context;Ljava/lang/String;)Laenu/ax360e/Emulator$GameInfo;", (void *) j_meta_info_from_zar_game}
            ,{"meta_info_from_xex_game", "(Landroid/content/Context;Ljava/lang/String;)Laenu/ax360e/Emulator$GameInfo;", (void *) j_meta_info_from_xex_game}
    };
    return env->RegisterNatives(g_class_Emulator,methods, sizeof(methods)/sizeof(methods[0]));
}
