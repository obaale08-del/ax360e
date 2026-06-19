// SPDX-License-Identifier: WTFPL
package aenu.ax360e;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.DialogInterface;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;
import android.view.Window;
import android.widget.Toast;

import androidx.activity.OnBackPressedCallback;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.Toolbar;
import androidx.fragment.app.DialogFragment;

import androidx.preference.Preference;
import androidx.preference.PreferenceDataStore;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceScreen;

import org.json.JSONObject;

import aenu.preference.CheckBoxPreference;
import aenu.preference.ListPreference;
import aenu.preference.SeekBarPreference;


import java.io.File;
import java.util.Set;

public class EmulatorSettings extends AppCompatActivity {

    static final String EXTRA_CONFIG_PATH="config_path";

    static final int WARNING_COLOR=0xffff8000;
    static final String Vulkan$vulkan_lib_path="Vulkan|vulkan_lib_path";

    static final int REQUEST_CODE_SELECT_CUSTOM_DRIVER=6101;
    @SuppressLint("ValidFragment")
    public static class SettingsFragment extends PreferenceFragmentCompat implements
            Preference.OnPreferenceClickListener,Preference.OnPreferenceChangeListener{

        boolean is_global;
        String config_path;
        Emulator.Config original_config;
        Emulator.Config config;
        PreferenceScreen root_pref;

        SettingsFragment(String config_path,boolean is_global){
            this.config_path=config_path;
            this.is_global=is_global;
        }

        OnBackPressedCallback back_callback=new OnBackPressedCallback(true) {
            @Override
            public void handleOnBackPressed() {
                String current=SettingsFragment.this.getPreferenceScreen().getKey();
                if (current==null){
                    requireActivity().finish();
                    return;
                }
                int p=current.lastIndexOf('|');
                if (p==-1)
                    setPreferenceScreen(root_pref);
                else
                    setPreferenceScreen(root_pref.findPreference(current.substring(0,p)));
            }
        };

        final PreferenceDataStore data_store=new PreferenceDataStore(){

            public void putString(String key, @Nullable String value) {
                if(config==null) return;
                config.save_config_entry(key,value);
            }

            public void putStringSet(String key, @Nullable Set<String> values) {
                throw new UnsupportedOperationException("Not implemented on this data store");
            }

            public void putInt(String key, int value) {
                if(config==null) return;
                config.save_config_entry(key,Integer.toString(value));
            }

            public void putLong(String key, long value) {
                throw new UnsupportedOperationException("Not implemented on this data store");
            }

            public void putFloat(String key, float value) {
                throw new UnsupportedOperationException("Not implemented on this data store");
            }

            public void putBoolean(String key, boolean value) {
                if(config==null) return;
                config.save_config_entry(key,Boolean.toString(value));
            }

            @Nullable
            public String getString(String key, @Nullable String defValue) {
                if(config==null) return defValue;
                return config.load_config_entry(key);
            }

            @Nullable
            public Set<String> getStringSet(String key, @Nullable Set<String> defValues) {
                //return defValues;
                throw new UnsupportedOperationException("Not implemented on this data store");
            }

            public int getInt(String key, int defValue) {
                if(config==null) return defValue;
                String v=config.load_config_entry(key);
                return v!=null?Integer.parseInt(v):defValue;
            }

            public long getLong(String key, long defValue) {
                throw new UnsupportedOperationException("Not implemented on this data store");
            }

            public float getFloat(String key, float defValue) {
                throw new UnsupportedOperationException("Not implemented on this data store");
            }

            public boolean getBoolean(String key, boolean defValue) {
                if(config==null) return defValue;
                String v=config.load_config_entry(key);
                return v!=null?Boolean.parseBoolean(v):defValue;
            }
        };

        Preference reset_as_default_pref(File _config_file){
            Preference p=new Preference(requireContext());
            p.setTitle(R.string.reset_as_default);
            p.setIconSpaceReserved(false);
            p.setOnPreferenceClickListener(new Preference.OnPreferenceClickListener(){
                public boolean onPreferenceClick(@NonNull Preference preference) {
                    new AlertDialog.Builder(requireContext())
                            .setMessage(getString(R.string.reset_as_default)+"?")
                            .setPositiveButton(android.R.string.ok, new DialogInterface.OnClickListener() {
                                @Override
                                public void onClick(DialogInterface dialog, int which) {
                                    if (config!=null) {
                                        config.close_config_file();
                                        config=null;
                                    }
                                    if(original_config!=null){
                                        original_config.close_config();
                                        original_config=null;
                                    }
                                    Utils.copy_file(Application.get_default_config_file(),_config_file);
                                    requireActivity().finish();
                                }
                            })
                            .setNegativeButton(android.R.string.cancel, null)
                            .create().show();
                    return true;
                }
            });
            return p;
        }

        Preference reset_as_global_pref(){
            Preference p=new Preference(requireContext());
            p.setTitle(R.string.use_global_config);
            p.setIconSpaceReserved(false);
            p.setOnPreferenceClickListener(new Preference.OnPreferenceClickListener(){
                public boolean onPreferenceClick(@NonNull Preference preference) {
                    new AlertDialog.Builder(requireContext())
                            .setMessage(getString(R.string.use_global_config)+"?")
                            .setPositiveButton(android.R.string.ok, new DialogInterface.OnClickListener() {
                                @Override
                                public void onClick(DialogInterface dialog, int which) {
                                    if (config!=null) {
                                        config.close_config_file();
                                        config=null;
                                    }
                                    if(original_config!=null){
                                        original_config.close_config();
                                        original_config=null;
                                    }

                                    new File(config_path).delete();
                                    requireActivity().finish();
                                }
                            })
                            .setNegativeButton(android.R.string.cancel, null)
                            .create().show();
                    return true;
                }
            });
            return p;
        }

        public void setPreferenceScreen(PreferenceScreen preferenceScreen){
            super.setPreferenceScreen(preferenceScreen);
            CharSequence title=preferenceScreen.getTitle();
            if(title==null)
                title=getString(R.string.settings);
            EmulatorSettings settings=(EmulatorSettings) requireActivity();
            if(settings.getSupportActionBar()!=null) {
                settings.getSupportActionBar().setTitle(title);
            }
        }

        @Override
        public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {

            if(rootKey!=null) throw new RuntimeException();
            getPreferenceManager().setPreferenceDataStore(data_store);
            setPreferencesFromResource(R.xml.emulator_settings, rootKey);
            root_pref=getPreferenceScreen();

            if(is_global) {
                root_pref.addPreference(reset_as_default_pref(Application.get_global_config_file()));
            }
            else{
                root_pref.addPreference(reset_as_default_pref(new File(config_path)));
                root_pref.addPreference(reset_as_global_pref());
            }

            requireActivity().getOnBackPressedDispatcher().addCallback(back_callback);

            if(!new File(config_path).exists()){
                root_pref.setEnabled(false);
                Toast.makeText(requireContext(), config_path, Toast.LENGTH_LONG).show();
                return;
            }

            try{
                config=Emulator.Config.open_config_file(config_path);
                original_config=Emulator.Config.open_config_from_string(Application.load_default_config_str(getContext()));
            }catch(Exception e){
                Log.e("EmulatorSettings",e.toString());
                root_pref.setEnabled(false);
                return;
            }



            final String[] BOOL_KEYS={
                    "APU|enable_xmp",
                    "APU|ffmpeg_verbose",
                    "APU|mute",
                    "APU|use_dedicated_xma_thread",
                    "CPU|break_condition_truncate",
                    "CPU|break_on_debugbreak",
                    "CPU|break_on_start",
                    "CPU|break_on_unimplemented_instructions",
                    "CPU|clock_no_scaling",
                    "CPU|clock_source_raw",
                    "CPU|disable_context_promotion",
                    "CPU|disable_instruction_infocache",
                    "CPU|disable_prefetch_and_cachecontrol",
                    "CPU|disassemble_functions",
                    "CPU|dump_translated_hir_functions",
                    "CPU|emit_inline_mmio_checks",
                    "CPU|emit_mmio_aware_stores_for_recorded_exception_addresses",
                    "CPU|enable_early_precompilation",
                    "CPU|full_optimization_even_with_debug",
                    "CPU|ignore_trap_instructions",
                    "CPU|inline_mmio_access",
                    "CPU|no_reserved_ops",
                    "CPU|permit_float_constant_evaluation",
                    "CPU|record_mmio_access_exceptions",
                    "CPU|store_all_context_values",
                    "CPU|trace_function_coverage",
                    "CPU|trace_function_references",
                    "CPU|trace_functions",
                    "CPU|validate_hir",
                    "CPU|writable_code_segments",
                    "Display|fullscreen",
                    "Display|host_present_from_non_ui_thread",
                    "Display|postprocess_dither",
                    "Display|present_letterbox",
                    "Display|present_render_pass_clear",
                    "GPU|clear_memory_page_state",
                    "GPU|depth_float24_convert_in_pixel_shader",
                    "GPU|depth_float24_round",
                    "GPU|depth_transfer_not_equal_test",
                    "GPU|disassemble_pm4",
                    "GPU|draw_resolution_scaled_texture_offsets",
                    "GPU|execute_unclipped_draw_vs_on_cpu",
                    "GPU|execute_unclipped_draw_vs_on_cpu_for_psi_render_backend",
                    "GPU|execute_unclipped_draw_vs_on_cpu_with_scissor",
                    "GPU|force_convert_line_loops_to_strips",
                    "GPU|force_convert_quad_lists_to_triangle_lists",
                    "GPU|force_convert_triangle_fans_to_lists",
                    "GPU|gamma_render_target_as_srgb",
                    "GPU|gpu_allow_invalid_fetch_constants",
                    "GPU|gpu_allow_invalid_upload_range",
                    "GPU|half_pixel_offset",
                    "GPU|ignore_32bit_vertex_index_support",
                    "GPU|log_guest_driven_gpu_register_written_values",
                    "GPU|log_ringbuffer_kickoff_initiator_bts",
                    "GPU|mrt_edram_used_range_clamp_to_min",
                    "GPU|native_2x_msaa",
                    "GPU|native_stencil_value_output",
                    "GPU|non_seamless_cube_map",
                    "GPU|readback_memexport",
                    "GPU|resolve_resolution_scale_fill_half_pixel_offset",
                    "GPU|snorm16_render_target_full_range",
                    "GPU|store_shaders",
                    "GPU|trace_gpu_stream",
                    "GPU|vsync",
                    "General|allow_game_relative_writes",
                    "General|allow_plugins",
                    "General|apply_patches",
                    "General|controller_hotkeys",
                    "General|debug",
                    "General|disable_doubleclick_fullscreen",
                    "General|discord",
                    "HID|guide_button",
                    "Kernel|allow_avatar_initialization",
                    "Kernel|allow_incompatible_title_update",
                    "Kernel|allow_nui_initialization",
                    "Kernel|apply_title_update",
                    "Kernel|ignore_thread_affinities",
                    "Kernel|ignore_thread_priorities",
                    "Kernel|kernel_cert_monitor",
                    "Kernel|kernel_debug_monitor",
                    "Kernel|kernel_pix",
                    "Kernel|log_high_frequency_kernel_calls",
                    "Kernel|staging_mode",
                    "Logging|flush_log",
                    "Logging|log_string_format_kernel_calls",
                    "Logging|log_to_debugprint",
                    "Logging|log_to_stdout",
                    "Memory|ignore_offset_for_ranged_allocations",
                    "Memory|protect_on_release",
                    "Memory|protect_zero",
                    "Memory|scribble_heap",
                    "Memory|writable_executable_memory",
                    "Storage|force_mount_devkit",
                    "Storage|mount_cache",
                    "Storage|mount_memory_unit",
                    "Storage|mount_scratch",
                    "UI|headless",
                    "UI|profiler_dpi_scaling",
                    "UI|show_achievement_notification",
                    "UI|show_profiler",
                    "UI|storage_selection_dialog",
                    "Video|enable_3d_mode",
                    "Video|interlaced",
                    "Video|use_50Hz_mode",
                    "Video|widescreen",
                    "Vulkan|adrenotools_force_max_clocks",
                    "Vulkan|vulkan_allow_present_mode_fifo_relaxed",
                    "Vulkan|vulkan_allow_present_mode_immediate",
                    "Vulkan|vulkan_allow_present_mode_mailbox",
                    "Vulkan|vulkan_log_debug_messages",
                    "Vulkan|vulkan_sparse_shared_memory",
                    "Vulkan|vulkan_validation",
            };
            final String[] INT_KEYS={
                    "APU|apu_max_queued_frames",
                    "APU|xmp_default_volume",
                    "GPU|texture_cache_memory_limit_hard",
                    "GPU|texture_cache_memory_limit_soft",
                    "General|time_scalar",
                    "Memory|mmap_address_high",
            };
            final String[] STRING_ARR_KEYS={
                    "APU|apu",
                    "APU|xma_decoder",
                    "CPU|cpu",
                    "Content|license_mask",
                    "Display|postprocess_antialiasing",
                    "Display|postprocess_scaling_and_sharpening",
                    "GPU|gpu",
                    "GPU|readback_resolve",
                    "GPU|render_target_path_vulkan",
                    "HID|hid",
                    "Kernel|kernel_display_gamma_type",
                    "Logging|log_level",
                    "Video|avpack",
                    "Video|internal_display_resolution",
                    "Video|video_standard",
                    "XConfig|user_country",
                    "XConfig|user_language",
            };


            final String[] NODE_KEYS={
                    "Vulkan",
                    "UI",
                    "Storage",
                    "Kernel",
                    "HID",
                    "Memory",
                    "XConfig",
                    "Display",
                    "GPU",
                    "Logging",
                    "APU",
                    "Content",
                    "CPU",
                    "General",
                    "Video"
            };


            for (String key:BOOL_KEYS){
                CheckBoxPreference pref=findPreference(key);
                String val_str=config.load_config_entry(key);
                if (val_str!=null) {
                    boolean val=Boolean.parseBoolean(val_str);
                    pref.setChecked(val);
                    setup_pref_title_color(pref,val_str);
                    //setup_config_dependency(pref,val_str);
                }
                pref.setOnPreferenceChangeListener(this);
                pref.setPreferenceDataStore(data_store);
            }

            for (String key:INT_KEYS){
                SeekBarPreference pref=findPreference(key);
                String val_str=config.load_config_entry(key);
                if (val_str!=null) {
                    //FIXME
                    try {
                        int val = Integer.parseInt(val_str);
                        pref.setValue(val);
                        setup_pref_title_color(pref,val_str);
                        //setup_config_dependency(pref,val_str);
                    } catch (NumberFormatException e) {
                        pref.setEnabled(false);
                    }
                }

                pref.setOnPreferenceChangeListener(this);
                pref.setPreferenceDataStore(data_store);
            }

            /* Preference.OnPreferenceChangeListener list_pref_change_listener=new Preference.OnPreferenceChangeListener() {
                @Override
                public boolean onPreferenceChange(@NonNull Preference preference, Object newValue) {
                    ListPreference pref=(ListPreference) preference;
                    CharSequence value=(CharSequence) newValue;
                    CharSequence[] values=pref.getEntryValues();
                    CharSequence[] entries=pref.getEntries();
                    for (int i=0;i<values.length;i++){
                        if (values[i].equals(value)){
                            pref.setSummary(entries[i]);
                            break;
                        }
                    }
                    return true;
                }
            };*/
            for (String key:STRING_ARR_KEYS){
                ListPreference pref=findPreference(key);
                String val_str=config.load_config_entry(key);
                if (val_str!=null) {
                    pref.setValue(val_str);
                    pref.setSummary(pref.getEntry());
                    setup_pref_title_color(pref,val_str);
                    //setup_config_dependency(pref,val_str);
                }
                pref.setOnPreferenceChangeListener(this);
                pref.setPreferenceDataStore(data_store);
            }

            for (String key:NODE_KEYS){
                PreferenceScreen pref=findPreference(key);
                pref.setOnPreferenceClickListener(this);
            }


            findPreference(Vulkan$vulkan_lib_path).setOnPreferenceClickListener(this);
            setup_costom_driver_library_path(config.load_config_entry(Vulkan$vulkan_lib_path));
            if(!Emulator.get.support_custom_driver()){
                findPreference(Vulkan$vulkan_lib_path).setEnabled(false);
                findPreference("Vulkan|adrenotools_force_max_clocks").setEnabled(false);
                //return;
            }
        }


        @Override
        public void onDisplayPreferenceDialog( @NonNull Preference pref) {
            if (pref instanceof SeekBarPreference) {
                final DialogFragment f = SeekBarPreference.SeekBarPreferenceFragmentCompat.newInstance(pref.getKey());
                f.setTargetFragment(this, 0);
                f.show(getParentFragmentManager(), "DIALOG_FRAGMENT_TAG");
                return;
            }
            super.onDisplayPreferenceDialog(pref);
        }

        @Override
        public void onDestroy() {
            super.onDestroy();
            if (config!=null)
                config.close_config_file();
        }

        /*@Override
        public boolean onPreferenceChange(Preference preference, Object newValue) {
            Log.i("onPreferenceChange",preference.getKey()+" "+newValue);
            if (preference instanceof CheckBoxPreference){
                config.save_config_entry(preference.getKey(),newValue.toString());
            }else if (preference instanceof ListPreference){
                config.save_config_entry(preference.getKey(),newValue.toString());
            }else if (preference instanceof SeekBarPreference){
                config.save_config_entry(preference.getKey(),newValue.toString());
            }
            return true;
        }*/


        @Override
        public boolean onPreferenceClick(@NonNull Preference preference) {
            if(preference.getKey().equals(Vulkan$vulkan_lib_path)){
                show_select_custom_driver_list();
                return false;
            }
            if(preference instanceof PreferenceScreen){
                setPreferenceScreen(root_pref.findPreference(preference.getKey()));
                return false;
            }
            return false;
        }

        void create_list_dialog(String title, String[] items, DialogInterface.OnClickListener listener){
            AlertDialog.Builder builder = new AlertDialog.Builder(getContext());
            builder.setTitle(title)
                    .setItems(items, listener)
                    .setNegativeButton(android.R.string.cancel, null);
            builder.create().show();
        }

        void show_select_custom_driver_list(){
            File[] files=Application.get_custom_driver_dir().listFiles();
            if(files==null||files.length==0){
                create_list_dialog(getString(R.string.es_vulkan_vulkan_lib_path)
                        , new String[]{getString(R.string._default),getString(R.string.driver_library_path_dialog_add_hint)}, new DialogInterface.OnClickListener() {
                            @Override
                            public void onClick(DialogInterface dialog, int which) {
                                dialog.dismiss();
                                if(which==0)
                                    config.save_config_entry(Vulkan$vulkan_lib_path,"default");//FIXME:
                                else
                                    request_select_custom_driver_file();
                            }
                        });
                return;
            }

            String  items[]=new String[files.length+2];
            items[0]=getString(R.string._default);
            for(int i=0;i<files.length;i++){
                if(files[i].isFile())
                    items[i+1]=files[i].getName();
                else{
                    File[] sub_files=files[i].listFiles();
                    if(sub_files.length==1)
                        items[i+1]=files[i].getName()+"/"+sub_files[0].getName();
                    else{
                        File json_f=new File(files[i], "meta.json");
                        if(json_f.exists()){
                            try {
                                JSONObject json = new JSONObject(Utils.read_file_as_str(json_f));
                                items[i+1]=files[i].getName()+"/"+json.getString("libraryName");
                            } catch (Exception e) {
                                items[i+1]="";
                            }
                        }
                        else
                            items[i+1]="";
                    }
                }
            }
            items[files.length+1]=getString(R.string.driver_library_path_dialog_add_hint);
            create_list_dialog(getString(R.string.es_vulkan_vulkan_lib_path), items, new DialogInterface.OnClickListener() {
                @Override
                public void onClick(DialogInterface dialog, int which) {
                    dialog.dismiss();
                    if(which==0){
                        config.save_config_entry(Vulkan$vulkan_lib_path,"default");//FIXME:
                        setup_costom_driver_library_path(null);
                    }
                    else if(which==files.length+1){
                        request_select_custom_driver_file();
                    }else{
                        File f=new File(files[which-1].getParentFile(),items[which]);
                        setup_costom_driver_library_path(f.getAbsolutePath());
                    }
                }
            });
        }
        void request_select_custom_driver_file(){
            Intent intent=new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("*/*");
            ((Activity)requireActivity()).startActivityForResult(intent, REQUEST_CODE_SELECT_CUSTOM_DRIVER);
        }
        void setup_pref_title_color(Preference preference,String cur_val){
            if(preference instanceof CheckBoxPreference){
                CheckBoxPreference pref=(CheckBoxPreference) preference;
                boolean modify=!original_config.load_config_entry(pref.getKey()).equals((cur_val));
                pref.set_is_modify_color(modify);
            }
            else if(preference instanceof SeekBarPreference){
                SeekBarPreference pref=(SeekBarPreference) preference;
                boolean modify=!original_config.load_config_entry(pref.getKey()).equals((cur_val));
                pref.set_is_modify_color(modify);
            }
            else if(preference instanceof ListPreference){
                ListPreference pref=(ListPreference) preference;
                boolean modify=!original_config.load_config_entry(pref.getKey()).equals((cur_val));
                pref.set_is_modify_color(modify);
            }
        }
        void setup_costom_driver_library_path(String new_path) {
            final String key=Vulkan$vulkan_lib_path;
            final String _default_path=getString(R.string._default);
            if(new_path==null||new_path.isEmpty()){
                findPreference( key).setSummary(_default_path);
                return;
            }

            config.save_config_entry(key,new_path);
            if(new_path.equals("default"))//FIXME:
                findPreference(key).setSummary(_default_path);
            else
                findPreference(key).setSummary(new_path);
        }
        @Override
        public boolean onPreferenceChange(@NonNull Preference preference, Object newValue) {
            if(preference instanceof CheckBoxPreference){
                CheckBoxPreference pref=(CheckBoxPreference) preference;
                String value=Boolean.toString((boolean)newValue);
                setup_pref_title_color(pref,value);
                //setup_config_dependency(pref,value);
                return true;
            }
            else if(preference instanceof SeekBarPreference){
                SeekBarPreference pref=(SeekBarPreference) preference;
                String value=Integer.toString((int)newValue);
                setup_pref_title_color(pref,value);
                //setup_config_dependency(pref,value);
                return true;
            }
            else if(preference instanceof ListPreference){
                ListPreference pref=(ListPreference) preference;
                CharSequence value=(CharSequence) newValue;
                CharSequence[] values=pref.getEntryValues();
                CharSequence[] entries=pref.getEntries();
                for (int i=0;i<values.length;i++){
                    if (values[i].equals(value)){
                        pref.setSummary(entries[i]);
                        break;
                    }
                }
                setup_pref_title_color(pref,value.toString());
                //setup_config_dependency(pref,value.toString());
                return true;
            }

            return false;
        }
    }

    SettingsFragment fragment;
    @Override
    protected void onCreate(Bundle savedInstanceState) {

        supportRequestWindowFeature(Window.FEATURE_NO_TITLE);
        super.onCreate(savedInstanceState);

        setContentView(R.layout.activity_emulator_settings);

        Toolbar toolbar = findViewById(R.id.toolbar);
        setSupportActionBar(toolbar);
        if(getSupportActionBar()!=null) {
            getSupportActionBar().setTitle(getString(R.string.settings));
        }
        toolbar.setNavigationOnClickListener(v -> onBackPressed());

        String config_path=getIntent().getStringExtra(EXTRA_CONFIG_PATH);


        if(config_path!=null) {
            fragment=new SettingsFragment(config_path,false);
        }
        else{
            fragment=new SettingsFragment(Application.get_global_config_file().getAbsolutePath(),true);
        }

        getSupportFragmentManager().beginTransaction().replace(R.id.settings_container,fragment).commit();
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, @Nullable Intent data) {
        super.onActivityResult(requestCode, resultCode, data);

        if (resultCode != RESULT_OK || data == null) return;

        Uri uri=data.getData();
        String file_name = Utils.getFileNameFromUri(uri);

        switch (requestCode){
            case REQUEST_CODE_SELECT_CUSTOM_DRIVER:
                if(file_name.endsWith(".zip"))
                    Utils.install_custom_driver_from_zip(this,uri,(path)->{ fragment.setup_costom_driver_library_path(path);});
                break;
        }
    }
}
