// Methods for RFID transmission

// lfrid
#include <lib/lfrfid/lfrfid_worker.h>
#include <toolbox/protocols/protocol_dict.h>
#include <lfrfid/protocols/lfrfid_protocols.h>
#include <lfrfid/lfrfid_raw_file.h>
#include <lib/toolbox/args.h>

#include <flipper_format/flipper_format.h>

#include "action_i.h"
#include "quac.h"

// lifted from flipperzero-firmware/applications/main/lfrfid/lfrfid_cli.c
void action_rfid_tx(void* context, FuriString* action_path, FuriString* error) {
    UNUSED(error);

    App* app = context;
    FuriString* file_name = action_path;

    FlipperFormat* fff_data_file = flipper_format_file_alloc(app->storage);
    FuriString* temp_str;
    temp_str = furi_string_alloc();
    uint32_t temp_data32;

    FuriString* protocol_name;
    FuriString* data_text;
    protocol_name = furi_string_alloc();
    data_text = furi_string_alloc();

    ProtocolDict* dict = protocol_dict_alloc(lfrfid_protocols, LFRFIDProtocolMax);
    ProtocolId protocol;
    size_t data_size = protocol_dict_get_max_data_size(dict);
    uint8_t* data = malloc(data_size);

    FURI_LOG_I(TAG, "Max dict data size is %d", data_size);
    bool successful_read = false;
    do {
        if(!flipper_format_file_open_existing(fff_data_file, furi_string_get_cstr(file_name))) {
            ACTION_SET_ERROR("RFID: Error opening %s", furi_string_get_cstr(file_name));
            break;
        }
        FURI_LOG_I(TAG, "Opened file");
        if(!flipper_format_read_header(fff_data_file, temp_str, &temp_data32)) {
            ACTION_SET_ERROR("RFID: Missing or incorrect header");
            break;
        }
        FURI_LOG_I(TAG, "Read file headers");
        // TODO: add better header checks here...
        if(!strcmp(furi_string_get_cstr(temp_str), "Flipper RFID key")) {
        } else {
            ACTION_SET_ERROR("RFID: Type or version mismatch");
            break;
        }

        // read and check the protocol field
        if(!flipper_format_read_string(fff_data_file, "Key type", protocol_name)) {
            ACTION_SET_ERROR("RFID: Error reading protocol");
            break;
        }
        protocol = protocol_dict_get_protocol_by_name(dict, furi_string_get_cstr(protocol_name));
        if(protocol == PROTOCOL_NO) {
            ACTION_SET_ERROR("RFID: Unknown protocol: %s", furi_string_get_cstr(protocol_name));
            break;
        }

        // read and check data field
        size_t required_size = protocol_dict_get_data_size(dict, protocol);
        FURI_LOG_I(TAG, "Protocol req data size is %d", required_size);
        if(!flipper_format_read_hex(fff_data_file, "Data", data, required_size)) {
            FURI_LOG_E(TAG, "Error reading data");
            ACTION_SET_ERROR("RFID: Error reading data");
            break;
        }
        // FURI_LOG_I(TAG, "Data: %s", furi_string_get_cstr(data_text));

        // if(data_size != required_size) {
        //     FURI_LOG_E(
        //         TAG,
        //         "%s data needs to be %zu bytes long",
        //         protocol_dict_get_name(dict, protocol),
        //         required_size);
        //     break;
        // }

        protocol_dict_set_data(dict, protocol, data, data_size);
        successful_read = true;
        FURI_LOG_I(TAG, "protocol dict setup complete!");
    } while(false);

    if(successful_read) {
        LFRFIDWorker* worker = lfrfid_worker_alloc(dict);

        lfrfid_worker_start_thread(worker);
        lfrfid_worker_emulate_start(worker, protocol);

        FURI_LOG_I(TAG, "Emulating RFID...");
        int16_t time_ms = 3000;
        int16_t interval_ms = 200;
        while(time_ms > 0) {
            furi_delay_ms(interval_ms);
            time_ms -= interval_ms;
        }
        FURI_LOG_I(TAG, "Emulation stopped");

        lfrfid_worker_stop(worker);
        lfrfid_worker_stop_thread(worker);
        lfrfid_worker_free(worker);
    }

    furi_string_free(temp_str);
    furi_string_free(protocol_name);
    furi_string_free(data_text);
    free(data);

    protocol_dict_free(dict);

    flipper_format_free(fff_data_file);
}
