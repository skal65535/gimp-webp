/**
 * gimp-webp - WebP Plugin for the GIMP
 * Copyright (C) 2015  Nathan Osman
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <libgimp/gimp.h>
#include <libgimp/gimpui.h>
#include <string.h>
#include <webp/encode.h>

#include "read-webp.h"
#include "write-webp.h"
#include "export-dialog.h"

const char BINARY_NAME[]    = "file-webp";
const char LOAD_PROCEDURE[] = "file-webp-load";
const char SAVE_PROCEDURE[] = "file-webp-save";

/* Predeclare our entrypoints. */
void query();
void run(const gchar *, gint, const GimpParam *, gint *, GimpParam **);

/* Declare our plugin entry points. */
GimpPlugInInfo PLUG_IN_INFO = {
    NULL,
    NULL,
    query,
    run
};

MAIN()

/* This function registers our load and save handlers. */
void query()
{
    /* Load arguments. */
    static const GimpParamDef load_arguments[] = {
        { GIMP_PDB_INT32,  "run-mode",     "Interactive, non-interactive" },
        { GIMP_PDB_STRING, "filename",     "The name of the file to load" },
        { GIMP_PDB_STRING, "raw-filename", "The name entered" }
    };

    /* Load return values. */
    static const GimpParamDef load_return_values[] = {
        { GIMP_PDB_IMAGE, "image", "Output image" }
    };

    /* Save arguments. */
    static const GimpParamDef save_arguments[] = {
        { GIMP_PDB_INT32,    "run-mode",     "Interactive, non-interactive" },
        { GIMP_PDB_IMAGE,    "image",        "Input image" },
        { GIMP_PDB_DRAWABLE, "drawable",     "Drawable to save" },
        { GIMP_PDB_STRING,   "filename",     "The name of the file to save the image to" },
        { GIMP_PDB_STRING,   "raw-filename", "The name entered" },
        { GIMP_PDB_FLOAT,    "quality",      "Quality of the image (0 <= quality <= 100)" }
    };

    /* Install the load procedure. */
    gimp_install_procedure(LOAD_PROCEDURE,
                           "Loads images in the WebP file format",
                           "Loads images in the WebP file format",
                           "Nathan Osman",
                           "Copyright Nathan Osman",
                           "2015",
                           "WebP image",
                           NULL,
                           GIMP_PLUGIN,
                           G_N_ELEMENTS(load_arguments),
                           G_N_ELEMENTS(load_return_values),
                           load_arguments,
                           load_return_values);

    /* Install the save procedure. */
    gimp_install_procedure(SAVE_PROCEDURE,
                           "Saves files in the WebP image format",
                           "Saves files in the WebP image format",
                           "Nathan Osman",
                           "Copyright Nathan Osman",
                           "2015",
                           "WebP image",
                           "RGB*",
                           GIMP_PLUGIN,
                           G_N_ELEMENTS(save_arguments),
                           0,
                           save_arguments,
                           NULL);

    /* Register the load handlers. */
    gimp_register_file_handler_mime(LOAD_PROCEDURE, "image/webp");
    gimp_register_load_handler(LOAD_PROCEDURE, "webp", "");

    /* Now register the save handlers. */
    gimp_register_file_handler_mime(SAVE_PROCEDURE, "image/webp");
    gimp_register_save_handler(SAVE_PROCEDURE, "webp", "");
}

/* This function is called when one of our methods is invoked. */
void run(const gchar * name,
         gint nparams,
         const GimpParam * param,
         gint * nreturn_vals,
         GimpParam ** return_vals)
{
    static GimpParam  values[1];
    GimpRunMode       run_mode;
    GimpPDBStatusType status = GIMP_PDB_SUCCESS;
    gint32            image_ID;
    gint32            drawable_ID;

    /* Determine the current run mode */
    run_mode = param[0].data.d_int32;

    /* Fill in the return values */
    *nreturn_vals = 1;
    *return_vals  = values;
    values[0].type          = GIMP_PDB_STATUS;
    values[0].data.d_status = GIMP_PDB_EXECUTION_ERROR;

    /* Determine which procedure is being invoked */
    if(!strcmp(name, LOAD_PROCEDURE)) {

        /* No need to determine whether the plugin is being invoked
         * interactively here since we don't need a UI for loading */

        image_ID = read_webp(param[1].data.d_string);

        if(image_ID != -1) {

            /* Return the new image that was loaded */
            *nreturn_vals = 2;
            values[1].type         = GIMP_PDB_IMAGE;
            values[1].data.d_image = image_ID;

        } else {
            status = GIMP_PDB_EXECUTION_ERROR;
        }

    } else if(!strcmp(name, SAVE_PROCEDURE)) {

        WebPConfig       config = {0};
        GimpExportReturn export_ret = GIMP_EXPORT_CANCEL;

        /* Load the image and drawable IDs */
        image_ID    = param[1].data.d_int32;
        drawable_ID = param[2].data.d_int32;

        /* Initialize the configuration */
        WebPConfigInit(&config);

        /* What happens next depends on the run mode */
        switch(run_mode) {
        case GIMP_RUN_INTERACTIVE:
        case GIMP_RUN_WITH_LAST_VALS:

            gimp_ui_init(BINARY_NAME, FALSE);

            /* Attempt to export the image */
            export_ret = gimp_export_image(&image_ID,
                                           &drawable_ID,
                                           "WEBP",
                                           GIMP_EXPORT_CAN_HANDLE_RGB | GIMP_EXPORT_CAN_HANDLE_ALPHA);

            /* Return immediately if canceled */
            if(export_ret == GIMP_EXPORT_CANCEL) {
                values[0].data.d_status = GIMP_PDB_CANCEL;
                return;
            }

            /* Display the export dialog */
            if(!export_dialog(&config)) {
                values[0].data.d_status = GIMP_PDB_CANCEL;
                return;
            }

        case GIMP_RUN_NONINTERACTIVE:

            /* Ensure the correct number of parameters were supplied */
            if(nparams != 6) {
                status = GIMP_PDB_CALLING_ERROR;
                break;
            }

            /* Apply the quality value */
            config.quality = param[5].data.d_float;

            break;
        }

        /* Attempt to save the image */
        if(!write_webp(param[3].data.d_string, drawable_ID, &config)) {
            status = GIMP_PDB_EXECUTION_ERROR;
        }
    }

    values[0].data.d_status = status;
}
