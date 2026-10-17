/*
 * NFC Info — Flipper Zero app (NFC + LF RFID).
 *
 * Fomenü:
 *   - NFC Scan : 13.56 MHz NFC olvasas (type/UID/SAK), Options menuvel
 *                (Save / Sectors-info / Replace / Save to new dir)
 *   - RFID Scan: 125 kHz LF RFID olvasas (protokoll + adat), menteni is
 *                lehet az /ext/lfrfid mappaba
 *   - NFC or RFID? : rovid utmutato, hogyan dontsd el, melyik a kartyad
 *   - About
 *
 * NFC: olvasas utan az Options menubol visszaterve NEM olvas ujra
 * (a talalat megmarad). A "Sectors" a mar beolvasott adatokbol keszit
 * info-oldalt (nem indit ujabb NFC-muveletet), ezert stabil.
 *
 * By: LeviXX
 */

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_rtc.h>
#include <datetime/datetime.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_box.h>
#include <gui/modules/text_input.h>
#include <dialogs/dialogs.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <nfc/nfc.h>
#include <nfc/nfc_poller.h>
#include <nfc/nfc_device.h>
#include <nfc/protocols/nfc_protocol.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a_poller.h>
#include <lfrfid/lfrfid_worker.h>
#include <lfrfid/lfrfid_dict_file.h>
#include <lfrfid/protocols/lfrfid_protocols.h>
#include <toolbox/protocols/protocol_dict.h>
#include <stdio.h>
#include <string.h>

#define TAG "NfcInfoApp"
#define APP_VERSION "v1.138"
#define NFC_INFO_SAVE_FOLDER "/ext/nfc"
#define LFRFID_SAVE_FOLDER "/ext/lfrfid"

typedef enum {
    NfcInfoViewSubmenu,
    NfcInfoViewNfcScan,
    NfcInfoViewOptionsMenu,
    NfcInfoViewTextInput,
    NfcInfoViewSectors,
    NfcInfoViewRfidScan,
    NfcInfoViewRfidOptions,
    NfcInfoViewRfidDetails,
    NfcInfoViewHowTo,
    NfcInfoViewAbout,
} NfcInfoView;

typedef enum {
    SubmenuIndexNfcScan,
    SubmenuIndexRfidScan,
    SubmenuIndexHowTo,
    SubmenuIndexAbout,
} SubmenuIndex;

typedef enum {
    OptionsMenuIndexSave,
    OptionsMenuIndexSectors,
    OptionsMenuIndexReplace,
    OptionsMenuIndexSaveToNewDir,
} OptionsMenuIndex;

typedef enum {
    RfidOptionsIndexDetails,
    RfidOptionsIndexSave,
    RfidOptionsIndexReplace,
    RfidOptionsIndexSaveToNewDir,
} RfidOptionsIndex;

/* Olvasasi fazis (mindket scenhez):
 *  0 = Trying to read (keres, nincs kartya)
 *  1 = Reading (kartya erzekelve, olvasas folyamatban)
 *  2 = Done reading (kesz) */
typedef enum {
    ReadPhaseTrying = 0,
    ReadPhaseReading = 1,
    ReadPhaseDone = 2,
} ReadPhase;

typedef struct {
    FuriMutex* mutex;

    /* NFC */
    bool card_found;
    bool card_saved;
    ReadPhase nfc_phase;
    char uid_str[40];
    char type_str[32];
    char atqa_str[16];
    char sak_str[8];
    Iso14443_3aData raw;

    /* RFID (LF) */
    bool rfid_found;
    bool rfid_saved;
    ReadPhase rfid_phase;
    char rfid_type_str[32];
    char rfid_data_str[64];
    ProtocolId rfid_protocol;
} NfcInfoState;

typedef struct NfcInfoApp {
    NfcInfoState* state;

    ViewDispatcher* view_dispatcher;
    Gui* gui;
    Submenu* submenu;
    Submenu* options_menu;
    Submenu* rfid_options_menu;
    View* nfc_view;
    View* rfid_view;
    TextBox* about_text_box;
    TextBox* sectors_text_box;
    TextBox* rfid_details_text_box;
    TextBox* howto_text_box;
    TextInput* text_input;
    DialogsApp* dialogs;

    NotificationApp* notifications;

    /* NFC */
    Nfc* nfc;
    NfcPoller* poller;
    FuriTimer* redraw_timer;

    /* RFID (LF) */
    ProtocolDict* lf_dict;
    LFRFIDWorker* lf_worker;
    bool lf_worker_running;
    FuriTimer* rfid_redraw_timer;

    char text_buffer[64];
    char sectors_buffer[512];
    bool saving_rfid; /* true: a TextInput mentes RFID-t ment, false: NFC-t */
} NfcInfoApp;

/* ---------- Dátum alapú alapnév ---------- */

static void make_date_name(char* buf, size_t size, const char* prefix) {
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    snprintf(
        buf,
        size,
        "%s_%04u-%02u-%02u_%02u-%02u-%02u",
        prefix,
        (unsigned)dt.year,
        (unsigned)dt.month,
        (unsigned)dt.day,
        (unsigned)dt.hour,
        (unsigned)dt.minute,
        (unsigned)dt.second);
}

/* ======================================================================
 *                            NFC rész
 * ====================================================================== */

static const char* guess_card_type(uint8_t sak) {
    switch(sak) {
    case 0x08:
        return "Mifare Classic 1K";
    case 0x18:
        return "Mifare Classic 4K";
    case 0x09:
        return "Mifare Mini";
    case 0x00:
        return "Mifare Ultralight/NTAG";
    case 0x20:
        return "ISO14443-4 compat.";
    case 0x28:
        return "Mifare Classic 1K";
    case 0x38:
        return "Mifare Classic 4K";
    default:
        return "ISO14443-3A card";
    }
}

static void nfc_view_draw_callback(Canvas* canvas, void* model) {
    NfcInfoApp* app = *(NfcInfoApp**)model;
    NfcInfoState* state = app->state;
    furi_mutex_acquire(state->mutex, FuriWaitForever);

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "NFC Scan");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 126, 10, AlignRight, AlignBottom, APP_VERSION);
    canvas_draw_line(canvas, 0, 13, 128, 13);

    if(state->card_found) {
        char line[48];

        canvas_draw_str(canvas, 2, 23, state->type_str);

        snprintf(line, sizeof(line), "UID: %s", state->uid_str);
        canvas_draw_str(canvas, 2, 33, line);

        snprintf(line, sizeof(line), "SAK: %s", state->sak_str);
        canvas_draw_str(canvas, 2, 43, line);

        if(state->card_saved) {
            canvas_draw_str(canvas, 2, 53, "Saved!");
        } else {
            canvas_draw_str(canvas, 2, 53, "Done reading!");
        }

        canvas_draw_str(canvas, 2, 63, "< Back");
        canvas_draw_str(canvas, 80, 63, "Options >");
    } else {
        canvas_draw_str(canvas, 2, 30, "Trying to read...");
        canvas_draw_str(canvas, 2, 40, "Place card on the back");
        canvas_draw_str(canvas, 2, 63, "Back: exit");
    }

    furi_mutex_release(state->mutex);
}

/* ---------- NFC mentés ---------- */

static void nfc_info_save_card_path(NfcInfoApp* app, const char* path) {
    NfcInfoState* state = app->state;

    furi_mutex_acquire(state->mutex, FuriWaitForever);
    Iso14443_3aData data_copy;
    memcpy(&data_copy, &state->raw, sizeof(Iso14443_3aData));
    furi_mutex_release(state->mutex);

    NfcDevice* device = nfc_device_alloc();
    nfc_device_set_data(device, NfcProtocolIso14443_3a, (const NfcDeviceData*)&data_copy);
    bool saved = nfc_device_save(device, path);
    nfc_device_free(device);

    furi_mutex_acquire(state->mutex, FuriWaitForever);
    state->card_saved = saved;
    furi_mutex_release(state->mutex);

    notification_message(
        app->notifications, saved ? &sequence_single_vibro : &sequence_error);
}

/* ---------- RFID mentés ---------- */

static void rfid_info_save_path(NfcInfoApp* app, const char* path) {
    NfcInfoState* state = app->state;

    furi_mutex_acquire(state->mutex, FuriWaitForever);
    ProtocolId proto = state->rfid_protocol;
    furi_mutex_release(state->mutex);

    bool saved = false;
    if(proto != PROTOCOL_NO) {
        saved = lfrfid_dict_file_save(app->lf_dict, proto, path);
    }

    furi_mutex_acquire(state->mutex, FuriWaitForever);
    state->rfid_saved = saved;
    furi_mutex_release(state->mutex);

    notification_message(
        app->notifications, saved ? &sequence_single_vibro : &sequence_error);
}

/* ---------- Közös TextInput mentés callback (NFC vagy RFID) ---------- */

static void text_input_callback(void* context) {
    NfcInfoApp* app = context;
    char path[128];

    if(app->saving_rfid) {
        if(strlen(app->text_buffer) > 0) {
            snprintf(path, sizeof(path), "%s/%s.rfid", LFRFID_SAVE_FOLDER, app->text_buffer);
        } else {
            char name[64];
            make_date_name(name, sizeof(name), "RFID");
            snprintf(path, sizeof(path), "%s/%s.rfid", LFRFID_SAVE_FOLDER, name);
        }
        rfid_info_save_path(app, path);
        view_dispatcher_switch_to_view(app->view_dispatcher, NfcInfoViewRfidScan);
    } else {
        if(strlen(app->text_buffer) > 0) {
            snprintf(path, sizeof(path), "%s/%s.nfc", NFC_INFO_SAVE_FOLDER, app->text_buffer);
        } else {
            char name[64];
            make_date_name(name, sizeof(name), "NFCInfo");
            snprintf(path, sizeof(path), "%s/%s.nfc", NFC_INFO_SAVE_FOLDER, name);
        }
        nfc_info_save_card_path(app, path);
        view_dispatcher_switch_to_view(app->view_dispatcher, NfcInfoViewNfcScan);
    }
}

static bool nfc_view_input_callback(InputEvent* event, void* context) {
    NfcInfoApp* app = context;
    NfcInfoState* state = app->state;

    if(event->type == InputTypeShort && event->key == InputKeyBack) {
        view_dispatcher_switch_to_view(app->view_dispatcher, NfcInfoViewSubmenu);
        return true;
    }

    if(event->type == InputTypeShort && event->key == InputKeyRight) {
        furi_mutex_acquire(state->mutex, FuriWaitForever);
        bool found = state->card_found;
        furi_mutex_release(state->mutex);

        if(found) {
            view_dispatcher_switch_to_view(app->view_dispatcher, NfcInfoViewOptionsMenu);
        }
        return true;
    }

    return false;
}

static NfcCommand poller_callback(NfcGenericEvent event, void* ctx) {
    NfcInfoApp* app = ctx;
    NfcInfoState* state = app->state;
    NfcCommand command = NfcCommandContinue;

    if(event.protocol == NfcProtocolIso14443_3a) {
        const Iso14443_3aPollerEvent* iso_event = event.event_data;
        if(iso_event->type == Iso14443_3aPollerEventTypeReady) {
            const Iso14443_3aData* data =
                (const Iso14443_3aData*)nfc_poller_get_data(app->poller);
            if(data) {
                furi_mutex_acquire(state->mutex, FuriWaitForever);

                memcpy(&state->raw, data, sizeof(Iso14443_3aData));

                state->uid_str[0] = '\0';
                size_t offset = 0;
                for(uint8_t i = 0; i < data->uid_len && i < 10; i++) {
                    int written = snprintf(
                        state->uid_str + offset,
                        sizeof(state->uid_str) - offset,
                        "%02X ",
                        data->uid[i]);
                    if(written <= 0 || (size_t)written >= sizeof(state->uid_str) - offset) break;
                    offset += (size_t)written;
                }

                snprintf(
                    state->atqa_str,
                    sizeof(state->atqa_str),
                    "%02X %02X",
                    data->atqa[0],
                    data->atqa[1]);
                snprintf(state->sak_str, sizeof(state->sak_str), "%02X", data->sak);
                snprintf(
                    state->type_str, sizeof(state->type_str), "%s", guess_card_type(data->sak));

                state->card_found = true;
                state->card_saved = false;

                furi_mutex_release(state->mutex);

                notification_message(app->notifications, &sequence_blink_stop);
                notification_message(app->notifications, &sequence_blink_start_green);
                notification_message(app->notifications, &sequence_success);

                command = NfcCommandStop;
            }
        }
    }

    return command;
}

static void redraw_timer_callback(void* context) {
    NfcInfoApp* app = context;
    view_commit_model(app->nfc_view, true);
}

static void nfc_view_enter_callback(void* context) {
    NfcInfoApp* app = context;

    /* Csak akkor inditunk uj olvasast, ha MEG NINCS talalat — igy az
     * Options menubol visszaterve nem olvas ujra. */
    bool need_scan;
    furi_mutex_acquire(app->state->mutex, FuriWaitForever);
    need_scan = !app->state->card_found;
    furi_mutex_release(app->state->mutex);

    app->redraw_timer = furi_timer_alloc(redraw_timer_callback, FuriTimerTypePeriodic, app);
    furi_timer_start(app->redraw_timer, furi_ms_to_ticks(200));

    if(need_scan) {
        notification_message(app->notifications, &sequence_blink_start_blue);
        app->poller = nfc_poller_alloc(app->nfc, NfcProtocolIso14443_3a);
        nfc_poller_start(app->poller, poller_callback, app);
    }
}

static void nfc_view_exit_callback(void* context) {
    NfcInfoApp* app = context;

    if(app->redraw_timer) {
        furi_timer_stop(app->redraw_timer);
        furi_timer_free(app->redraw_timer);
        app->redraw_timer = NULL;
    }

    notification_message(app->notifications, &sequence_blink_stop);

    if(app->poller) {
        nfc_poller_stop(app->poller);
        nfc_poller_free(app->poller);
        app->poller = NULL;
    }
}

/* ======================================================================
 *                            RFID (LF) rész
 * ====================================================================== */

static void rfid_view_draw_callback(Canvas* canvas, void* model) {
    NfcInfoApp* app = *(NfcInfoApp**)model;
    NfcInfoState* state = app->state;
    furi_mutex_acquire(state->mutex, FuriWaitForever);

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 2, 10, "RFID (125 kHz)");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_line(canvas, 0, 13, 128, 13);

    if(state->rfid_found) {
        char line[48];

        snprintf(line, sizeof(line), "Type: %s", state->rfid_type_str);
        canvas_draw_str(canvas, 2, 25, line);

        if(state->rfid_saved) {
            canvas_draw_str_aligned(canvas, 126, 25, AlignRight, AlignBottom, "Saved!");
        }

        canvas_draw_str(canvas, 2, 37, "Data:");
        canvas_draw_str(canvas, 2, 48, state->rfid_data_str);

        canvas_draw_str(canvas, 2, 63, "< Back");
        canvas_draw_str(canvas, 80, 63, "Options >");
    } else {
        const char* phase_msg = "Trying to read...";
        if(state->rfid_phase == ReadPhaseReading) {
            phase_msg = "Reading...";
        }
        canvas_draw_str(canvas, 2, 30, phase_msg);
        canvas_draw_str(canvas, 2, 40, "Place LF card on the back");
        canvas_draw_str(canvas, 2, 63, "Back: exit");
    }

    furi_mutex_release(state->mutex);
}

static bool rfid_view_input_callback(InputEvent* event, void* context) {
    NfcInfoApp* app = context;
    NfcInfoState* state = app->state;

    if(event->type == InputTypeShort && event->key == InputKeyBack) {
        view_dispatcher_switch_to_view(app->view_dispatcher, NfcInfoViewSubmenu);
        return true;
    }

    if(event->type == InputTypeShort && event->key == InputKeyRight) {
        furi_mutex_acquire(state->mutex, FuriWaitForever);
        bool found = state->rfid_found;
        furi_mutex_release(state->mutex);

        if(found) {
            view_dispatcher_switch_to_view(app->view_dispatcher, NfcInfoViewRfidOptions);
        }
        return true;
    }

    return false;
}

/* LF worker olvasas callback — a worker sajat szalan fut. */
static void rfid_read_callback(LFRFIDWorkerReadResult result, ProtocolId protocol, void* context) {
    NfcInfoApp* app = context;
    NfcInfoState* state = app->state;

    /* Fazis-jelzesek: kartya kozeledik -> "Reading", elmegy -> "Trying". */
    if(result == LFRFIDWorkerReadSenseCardStart) {
        furi_mutex_acquire(state->mutex, FuriWaitForever);
        state->rfid_phase = ReadPhaseReading;
        furi_mutex_release(state->mutex);
        return;
    }
    if(result == LFRFIDWorkerReadSenseCardEnd) {
        furi_mutex_acquire(state->mutex, FuriWaitForever);
        if(!state->rfid_found) state->rfid_phase = ReadPhaseTrying;
        furi_mutex_release(state->mutex);
        return;
    }

    if(result == LFRFIDWorkerReadDone && protocol != PROTOCOL_NO) {
        const char* name = protocol_dict_get_name(app->lf_dict, protocol);

        FuriString* data_s = furi_string_alloc();
        protocol_dict_render_brief_data(app->lf_dict, data_s, protocol);

        furi_mutex_acquire(state->mutex, FuriWaitForever);
        state->rfid_protocol = protocol;
        snprintf(state->rfid_type_str, sizeof(state->rfid_type_str), "%s", name ? name : "?");
        snprintf(
            state->rfid_data_str,
            sizeof(state->rfid_data_str),
            "%s",
            furi_string_get_cstr(data_s));
        state->rfid_found = true;
        state->rfid_saved = false;
        state->rfid_phase = ReadPhaseDone;
        furi_mutex_release(state->mutex);

        furi_string_free(data_s);

        notification_message(app->notifications, &sequence_blink_stop);
        notification_message(app->notifications, &sequence_blink_start_green);
        notification_message(app->notifications, &sequence_success);
    }
}

static void rfid_redraw_timer_callback(void* context) {
    NfcInfoApp* app = context;
    view_commit_model(app->rfid_view, true);
}

static void rfid_view_enter_callback(void* context) {
    NfcInfoApp* app = context;

    bool need_scan;
    furi_mutex_acquire(app->state->mutex, FuriWaitForever);
    need_scan = !app->state->rfid_found;
    furi_mutex_release(app->state->mutex);

    app->rfid_redraw_timer =
        furi_timer_alloc(rfid_redraw_timer_callback, FuriTimerTypePeriodic, app);
    furi_timer_start(app->rfid_redraw_timer, furi_ms_to_ticks(200));

    if(need_scan) {
        notification_message(app->notifications, &sequence_blink_start_blue);
        app->lf_worker = lfrfid_worker_alloc(app->lf_dict);
        lfrfid_worker_start_thread(app->lf_worker);
        lfrfid_worker_read_start(app->lf_worker, LFRFIDWorkerReadTypeAuto, rfid_read_callback, app);
        app->lf_worker_running = true;
    }
}

static void rfid_view_exit_callback(void* context) {
    NfcInfoApp* app = context;

    if(app->rfid_redraw_timer) {
        furi_timer_stop(app->rfid_redraw_timer);
        furi_timer_free(app->rfid_redraw_timer);
        app->rfid_redraw_timer = NULL;
    }

    notification_message(app->notifications, &sequence_blink_stop);

    if(app->lf_worker) {
        if(app->lf_worker_running) {
            lfrfid_worker_stop(app->lf_worker);
            app->lf_worker_running = false;
        }
        lfrfid_worker_stop_thread(app->lf_worker);
        lfrfid_worker_free(app->lf_worker);
        app->lf_worker = NULL;
    }
}

/* ======================================================================
 *                        Navigáció / menük
 * ====================================================================== */

static uint32_t about_previous_callback(void* context) {
    UNUSED(context);
    return NfcInfoViewSubmenu;
}

static uint32_t howto_previous_callback(void* context) {
    UNUSED(context);
    return NfcInfoViewSubmenu;
}

static uint32_t sectors_previous_callback(void* context) {
    UNUSED(context);
    return NfcInfoViewOptionsMenu;
}

static uint32_t options_menu_previous_callback(void* context) {
    UNUSED(context);
    return NfcInfoViewNfcScan;
}

static uint32_t rfid_options_previous_callback(void* context) {
    UNUSED(context);
    return NfcInfoViewRfidScan;
}

static uint32_t rfid_details_previous_callback(void* context) {
    UNUSED(context);
    return NfcInfoViewRfidOptions;
}

/* ---------- NFC Options menü ---------- */

static void options_menu_callback(void* context, uint32_t index) {
    NfcInfoApp* app = context;

    if(index == OptionsMenuIndexSave) {
        app->saving_rfid = false;
        app->text_buffer[0] = '\0';
        text_input_set_header_text(app->text_input, "Name (empty = date/UID):");
        text_input_set_result_callback(
            app->text_input,
            text_input_callback,
            app,
            app->text_buffer,
            sizeof(app->text_buffer),
            true);
        view_dispatcher_switch_to_view(app->view_dispatcher, NfcInfoViewTextInput);

    } else if(index == OptionsMenuIndexSectors) {
        furi_mutex_acquire(app->state->mutex, FuriWaitForever);
        snprintf(
            app->sectors_buffer,
            sizeof(app->sectors_buffer),
            "Card Info  " APP_VERSION "\n"
            "----------------\n"
            "Protocol: ISO14443-3A\n"
            "Card Type: %s\n"
            "UID: %s\n"
            "ATQA: %s\n"
            "SAK: %s\n"
            "UID Length: %d bytes\n\n"
            "Note: block/sector data\n"
            "lives on the card and\n"
            "needs the stock NFC app\n"
            "to dump.\n",
            app->state->type_str,
            app->state->uid_str,
            app->state->atqa_str,
            app->state->sak_str,
            app->state->raw.uid_len);
        furi_mutex_release(app->state->mutex);

        text_box_set_text(app->sectors_text_box, app->sectors_buffer);
        text_box_set_focus(app->sectors_text_box, TextBoxFocusStart);
        view_dispatcher_switch_to_view(app->view_dispatcher, NfcInfoViewSectors);

    } else if(index == OptionsMenuIndexReplace) {
        FuriString* file_path = furi_string_alloc_set(NFC_INFO_SAVE_FOLDER);
        DialogsFileBrowserOptions browser_options;
        dialog_file_browser_set_basic_options(&browser_options, ".nfc", NULL);

        bool result =
            dialog_file_browser_show(app->dialogs, file_path, file_path, &browser_options);
        if(result) {
            nfc_info_save_card_path(app, furi_string_get_cstr(file_path));
        }
        furi_string_free(file_path);
        view_dispatcher_switch_to_view(app->view_dispatcher, NfcInfoViewNfcScan);

    } else if(index == OptionsMenuIndexSaveToNewDir) {
        FuriString* dir_path = furi_string_alloc_set(NFC_INFO_SAVE_FOLDER);
        DialogsFileBrowserOptions browser_options;
        dialog_file_browser_set_basic_options(&browser_options, "", NULL);

        bool result = dialog_file_browser_show(app->dialogs, dir_path, dir_path, &browser_options);
        if(result) {
            char full_path[160];
            char name[64];
            make_date_name(name, sizeof(name), "NFCInfo");
            snprintf(
                full_path, sizeof(full_path), "%s/%s.nfc", furi_string_get_cstr(dir_path), name);
            nfc_info_save_card_path(app, full_path);
        }
        furi_string_free(dir_path);
        view_dispatcher_switch_to_view(app->view_dispatcher, NfcInfoViewNfcScan);
    }
}

/* ---------- RFID Options menü ---------- */

static void rfid_options_callback(void* context, uint32_t index) {
    NfcInfoApp* app = context;

    if(index == RfidOptionsIndexDetails) {
        /* Reszletes RFID info: protokoll, gyarto, teljes adat, meret. */
        furi_mutex_acquire(app->state->mutex, FuriWaitForever);
        ProtocolId proto = app->state->rfid_protocol;
        char type_copy[32];
        snprintf(type_copy, sizeof(type_copy), "%s", app->state->rfid_type_str);
        furi_mutex_release(app->state->mutex);

        FuriString* full = furi_string_alloc();
        const char* manuf = "";
        size_t data_size = 0;
        if(proto != PROTOCOL_NO) {
            protocol_dict_render_data(app->lf_dict, full, proto);
            manuf = protocol_dict_get_manufacturer(app->lf_dict, proto);
            data_size = protocol_dict_get_data_size(app->lf_dict, proto);
        }

        snprintf(
            app->sectors_buffer,
            sizeof(app->sectors_buffer),
            "RFID Details  " APP_VERSION "\n"
            "----------------\n"
            "Protocol: %s\n"
            "Manufacturer: %s\n"
            "Data size: %u bytes\n"
            "----------------\n"
            "%s\n",
            type_copy,
            (manuf && manuf[0]) ? manuf : "-",
            (unsigned)data_size,
            furi_string_get_cstr(full));

        furi_string_free(full);

        text_box_set_text(app->rfid_details_text_box, app->sectors_buffer);
        text_box_set_focus(app->rfid_details_text_box, TextBoxFocusStart);
        view_dispatcher_switch_to_view(app->view_dispatcher, NfcInfoViewRfidDetails);

    } else if(index == RfidOptionsIndexSave) {
        app->saving_rfid = true;
        app->text_buffer[0] = '\0';
        text_input_set_header_text(app->text_input, "Name (empty = date):");
        text_input_set_result_callback(
            app->text_input,
            text_input_callback,
            app,
            app->text_buffer,
            sizeof(app->text_buffer),
            true);
        view_dispatcher_switch_to_view(app->view_dispatcher, NfcInfoViewTextInput);

    } else if(index == RfidOptionsIndexReplace) {
        /* Meglevo .rfid fajl kivalasztasa, tartalmanak lecserelese a
         * most beolvasott kartyara (a fajl neve/helye marad). */
        FuriString* file_path = furi_string_alloc_set(LFRFID_SAVE_FOLDER);
        DialogsFileBrowserOptions browser_options;
        dialog_file_browser_set_basic_options(&browser_options, ".rfid", NULL);

        bool result =
            dialog_file_browser_show(app->dialogs, file_path, file_path, &browser_options);
        if(result) {
            rfid_info_save_path(app, furi_string_get_cstr(file_path));
        }
        furi_string_free(file_path);
        view_dispatcher_switch_to_view(app->view_dispatcher, NfcInfoViewRfidScan);

    } else if(index == RfidOptionsIndexSaveToNewDir) {
        FuriString* dir_path = furi_string_alloc_set(LFRFID_SAVE_FOLDER);
        DialogsFileBrowserOptions browser_options;
        dialog_file_browser_set_basic_options(&browser_options, "", NULL);

        bool result = dialog_file_browser_show(app->dialogs, dir_path, dir_path, &browser_options);
        if(result) {
            char full_path[160];
            char name[64];
            make_date_name(name, sizeof(name), "RFID");
            snprintf(
                full_path, sizeof(full_path), "%s/%s.rfid", furi_string_get_cstr(dir_path), name);
            rfid_info_save_path(app, full_path);
        }
        furi_string_free(dir_path);
        view_dispatcher_switch_to_view(app->view_dispatcher, NfcInfoViewRfidScan);
    }
}

/* ---------- Főmenü ---------- */

static void submenu_callback(void* context, uint32_t index) {
    NfcInfoApp* app = context;
    if(index == SubmenuIndexNfcScan) {
        /* Uj NFC keresest inditunk: elozo talalat torlese. */
        furi_mutex_acquire(app->state->mutex, FuriWaitForever);
        app->state->card_found = false;
        app->state->card_saved = false;
        app->state->nfc_phase = ReadPhaseTrying;
        furi_mutex_release(app->state->mutex);
        view_dispatcher_switch_to_view(app->view_dispatcher, NfcInfoViewNfcScan);
    } else if(index == SubmenuIndexRfidScan) {
        furi_mutex_acquire(app->state->mutex, FuriWaitForever);
        app->state->rfid_found = false;
        app->state->rfid_saved = false;
        app->state->rfid_phase = ReadPhaseTrying;
        furi_mutex_release(app->state->mutex);
        view_dispatcher_switch_to_view(app->view_dispatcher, NfcInfoViewRfidScan);
    } else if(index == SubmenuIndexHowTo) {
        view_dispatcher_switch_to_view(app->view_dispatcher, NfcInfoViewHowTo);
    } else if(index == SubmenuIndexAbout) {
        view_dispatcher_switch_to_view(app->view_dispatcher, NfcInfoViewAbout);
    }
}

static bool nfc_info_navigation_event_callback(void* context) {
    NfcInfoApp* app = context;
    view_dispatcher_stop(app->view_dispatcher);
    return true;
}

/* ======================================================================
 *                        App lifecycle
 * ====================================================================== */

static NfcInfoApp* nfc_info_app_alloc(void) {
    NfcInfoApp* app = malloc(sizeof(NfcInfoApp));
    memset(app, 0, sizeof(NfcInfoApp));

    app->state = malloc(sizeof(NfcInfoState));
    memset(app->state, 0, sizeof(NfcInfoState));
    app->state->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->state->rfid_protocol = PROTOCOL_NO;

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->dialogs = furi_record_open(RECORD_DIALOGS);
    app->nfc = nfc_alloc();
    app->lf_dict = protocol_dict_alloc(lfrfid_protocols, LFRFIDProtocolMax);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, nfc_info_navigation_event_callback);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    /* Főmenü */
    app->submenu = submenu_alloc();
    submenu_add_item(app->submenu, "NFC Scan", SubmenuIndexNfcScan, submenu_callback, app);
    submenu_add_item(app->submenu, "RFID Scan (125 kHz)", SubmenuIndexRfidScan, submenu_callback, app);
    submenu_add_item(app->submenu, "NFC or RFID?", SubmenuIndexHowTo, submenu_callback, app);
    submenu_add_item(app->submenu, "About", SubmenuIndexAbout, submenu_callback, app);
    view_dispatcher_add_view(
        app->view_dispatcher, NfcInfoViewSubmenu, submenu_get_view(app->submenu));

    /* NFC Options menü */
    app->options_menu = submenu_alloc();
    submenu_add_item(app->options_menu, "Save", OptionsMenuIndexSave, options_menu_callback, app);
    submenu_add_item(
        app->options_menu, "Details", OptionsMenuIndexSectors, options_menu_callback, app);
    submenu_add_item(
        app->options_menu, "Replace", OptionsMenuIndexReplace, options_menu_callback, app);
    submenu_add_item(
        app->options_menu,
        "Save to new dir",
        OptionsMenuIndexSaveToNewDir,
        options_menu_callback,
        app);
    view_set_previous_callback(submenu_get_view(app->options_menu), options_menu_previous_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, NfcInfoViewOptionsMenu, submenu_get_view(app->options_menu));

    /* RFID Options menü */
    app->rfid_options_menu = submenu_alloc();
    submenu_add_item(
        app->rfid_options_menu, "Details", RfidOptionsIndexDetails, rfid_options_callback, app);
    submenu_add_item(
        app->rfid_options_menu, "Save", RfidOptionsIndexSave, rfid_options_callback, app);
    submenu_add_item(
        app->rfid_options_menu, "Replace", RfidOptionsIndexReplace, rfid_options_callback, app);
    submenu_add_item(
        app->rfid_options_menu,
        "Save to new dir",
        RfidOptionsIndexSaveToNewDir,
        rfid_options_callback,
        app);
    view_set_previous_callback(
        submenu_get_view(app->rfid_options_menu), rfid_options_previous_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, NfcInfoViewRfidOptions, submenu_get_view(app->rfid_options_menu));

    /* NFC scan nézet */
    app->nfc_view = view_alloc();
    view_allocate_model(app->nfc_view, ViewModelTypeLockFree, sizeof(NfcInfoApp*));
    NfcInfoApp** nfc_model = view_get_model(app->nfc_view);
    *nfc_model = app;
    view_set_context(app->nfc_view, app);
    view_set_draw_callback(app->nfc_view, nfc_view_draw_callback);
    view_set_input_callback(app->nfc_view, nfc_view_input_callback);
    view_set_enter_callback(app->nfc_view, nfc_view_enter_callback);
    view_set_exit_callback(app->nfc_view, nfc_view_exit_callback);
    view_dispatcher_add_view(app->view_dispatcher, NfcInfoViewNfcScan, app->nfc_view);

    /* RFID scan nézet */
    app->rfid_view = view_alloc();
    view_allocate_model(app->rfid_view, ViewModelTypeLockFree, sizeof(NfcInfoApp*));
    NfcInfoApp** rfid_model = view_get_model(app->rfid_view);
    *rfid_model = app;
    view_set_context(app->rfid_view, app);
    view_set_draw_callback(app->rfid_view, rfid_view_draw_callback);
    view_set_input_callback(app->rfid_view, rfid_view_input_callback);
    view_set_enter_callback(app->rfid_view, rfid_view_enter_callback);
    view_set_exit_callback(app->rfid_view, rfid_view_exit_callback);
    view_dispatcher_add_view(app->view_dispatcher, NfcInfoViewRfidScan, app->rfid_view);

    /* Text Input */
    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, NfcInfoViewTextInput, text_input_get_view(app->text_input));

    /* Sectors info nézet (NFC) */
    app->sectors_text_box = text_box_alloc();
    view_set_previous_callback(
        text_box_get_view(app->sectors_text_box), sectors_previous_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, NfcInfoViewSectors, text_box_get_view(app->sectors_text_box));

    /* RFID Details info nézet */
    app->rfid_details_text_box = text_box_alloc();
    view_set_previous_callback(
        text_box_get_view(app->rfid_details_text_box), rfid_details_previous_callback);
    view_dispatcher_add_view(
        app->view_dispatcher,
        NfcInfoViewRfidDetails,
        text_box_get_view(app->rfid_details_text_box));

    /* How-to nézet */
    app->howto_text_box = text_box_alloc();
    text_box_set_text(
        app->howto_text_box,
        "NFC or RFID?\n"
        "----------------\n"
        "How to tell which one\n"
        "your card/tag is:\n"
        "\n"
        "1) Try NFC Scan first.\n"
        "   If it reads (shows a\n"
        "   UID), it's a 13.56 MHz\n"
        "   NFC card (Mifare, NTAG,\n"
        "   bank card, most phones,\n"
        "   newer access cards).\n"
        "\n"
        "2) If NFC finds nothing,\n"
        "   try RFID Scan. If that\n"
        "   reads, it's a 125 kHz\n"
        "   LF card (EM4100, HID,\n"
        "   Indala - older building\n"
        "   access fobs, pet chips,\n"
        "   many white blank cards).\n"
        "\n"
        "Rule of thumb:\n"
        "- Thin bank-style / phone\n"
        "  tap  -> usually NFC\n"
        "- Thick fob / old office\n"
        "  badge -> often RFID\n"
        "\n"
        "Some cards are dual; then\n"
        "both scans find something.\n");
    text_box_set_focus(app->howto_text_box, TextBoxFocusStart);
    view_set_previous_callback(text_box_get_view(app->howto_text_box), howto_previous_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, NfcInfoViewHowTo, text_box_get_view(app->howto_text_box));

    /* About nézet */
    app->about_text_box = text_box_alloc();
    text_box_set_text(
        app->about_text_box,
        "ScanAll  " APP_VERSION "\n"
        "----------------\n"
        "NFC + LF RFID reader and\n"
        "saving utility.\n"
        "\n"
        "Features:\n"
        "- NFC Scan: 13.56 MHz\n"
        "- RFID Scan: 125 kHz LF\n"
        "- On a hit: RIGHT arrow\n"
        "  opens Options (Save etc.)\n"
        "- NFC saves to /ext/nfc\n"
        "- RFID saves to /ext/lfrfid\n"
        "- Options->back keeps the\n"
        "  current read (no rescan)\n"
        "\n"
        "Version: " APP_VERSION "\n"
        "By: LeviXX\n"
        "\n"
        "Thanks for using it! :)\n");
    text_box_set_focus(app->about_text_box, TextBoxFocusStart);
    view_set_previous_callback(text_box_get_view(app->about_text_box), about_previous_callback);
    view_dispatcher_add_view(
        app->view_dispatcher, NfcInfoViewAbout, text_box_get_view(app->about_text_box));

    view_dispatcher_switch_to_view(app->view_dispatcher, NfcInfoViewSubmenu);

    return app;
}

static void nfc_info_app_free(NfcInfoApp* app) {
    view_dispatcher_remove_view(app->view_dispatcher, NfcInfoViewAbout);
    text_box_free(app->about_text_box);

    view_dispatcher_remove_view(app->view_dispatcher, NfcInfoViewHowTo);
    text_box_free(app->howto_text_box);

    view_dispatcher_remove_view(app->view_dispatcher, NfcInfoViewSectors);
    text_box_free(app->sectors_text_box);

    view_dispatcher_remove_view(app->view_dispatcher, NfcInfoViewRfidDetails);
    text_box_free(app->rfid_details_text_box);

    view_dispatcher_remove_view(app->view_dispatcher, NfcInfoViewTextInput);
    text_input_free(app->text_input);

    view_dispatcher_remove_view(app->view_dispatcher, NfcInfoViewRfidScan);
    view_free(app->rfid_view);

    view_dispatcher_remove_view(app->view_dispatcher, NfcInfoViewNfcScan);
    view_free(app->nfc_view);

    view_dispatcher_remove_view(app->view_dispatcher, NfcInfoViewRfidOptions);
    submenu_free(app->rfid_options_menu);

    view_dispatcher_remove_view(app->view_dispatcher, NfcInfoViewOptionsMenu);
    submenu_free(app->options_menu);

    view_dispatcher_remove_view(app->view_dispatcher, NfcInfoViewSubmenu);
    submenu_free(app->submenu);

    view_dispatcher_free(app->view_dispatcher);

    protocol_dict_free(app->lf_dict);
    nfc_free(app->nfc);

    furi_record_close(RECORD_DIALOGS);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);

    furi_mutex_free(app->state->mutex);
    free(app->state);
    free(app);
}

int32_t scanall_app(void* p) {
    UNUSED(p);
    NfcInfoApp* app = nfc_info_app_alloc();
    view_dispatcher_run(app->view_dispatcher);
    nfc_info_app_free(app);
    return 0;
}
