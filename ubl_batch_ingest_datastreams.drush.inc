<?php
/**
 * @file
 * Functions for batch ingesting datastreams via drush.
 */

//module_load_include('inc', 'ubl_check_datastreams', 'includes/ubl_check_datastreams.children');

/**
 * Implements hook_drush_command().
 */
function ubl_batch_ingest_datastreams_drush_command() {
  $items['batch_ingest_datastreams'] = array(
    'description' => 'Ingests or generates datastreams for objects found in a CSV file.',
    'options' => array(
      'csvfile' => 'A absolute path to an existing CSV file, containing lines with 3 values: an object id, a datastream id and optionally a absolute path to a datastream file.',
    ),
    'aliases' => array('bid'),
    'examples' => array(
      'drush --user=admin batch_ingest_datastreams --csvfile=/tmp/test.csv',
      'drush --user=admin bid --csvfile=/tmp/test.csv',
    ),
  );
  return $items;
}

/**
 * Implements drush_hook_COMMAND_validate().
 */
function drush_ubl_batch_ingest_datastreams_batch_ingest_datastreams_validate() {
  $csvfile = drush_get_option('csvfile');

  if (substr($csvfile, 0, 1) !== DIRECTORY_SEPARATOR) {
    return drush_set_error("csvfile should be an absolute path to an existing CSV file, but is not absolute."); 
  }
  if (!is_file($csvfile)) {
    return drush_set_error("csvfile should be an absolute path to an existing CSV file, but does not exist."); 
  }

  return TRUE; 
}

/**
 * Implements drush_hook_COMMAND().
 */
function drush_ubl_batch_ingest_datastreams_batch_ingest_datastreams() {
  module_load_include('inc', 'ubl_batch_ingest_datastreams', 'includes/ubl_batch_ingest_datastreams_zip_import.form');
  $csvfile = drush_get_option('csvfile');

  // TODO: below is simular to zip_import.form code. Make one function for this.
  ini_set("auto_detect_line_endings", "1");
  $delimit = "\t";
  $ingest_objects = array();
  $linenr = 0;
  $haserror = FALSE;
  if (($fh = fopen($csvfile, "r")) !== FALSE) {
    while (($data = fgetcsv($fh, 0, $delimit)) !== FALSE) {
      $linenr++;
      if ($linenr == 1) {
        // Treat header special.
        if (count($data) == 1) {
          $delimit = ",";
          $data = str_getcsv($data[0], $delimit);
        }
        if (count($data) == 1) {
          $delimit = ";";
          $data = str_getcsv($data[0], $delimit);
        }
        $objectid = $data[0];

        $foundobject = find_fedora_object_ids($objectid);

        if (count($foundobject) != 1) {
          // This is a header, so skip it.
          continue;
        }
      }

      if (count($data) == 1 && trim($data[0]) === "") {
        continue; // Just skip an empty
      }
      if (count($data) != 3 && count($data) != 2) {
        drush_log(t("Line @linenr has @countcolumn columns, but should be 3",
          array(
            "@linenr" => $linenr,
            "@countcolumn" => count($data),
          )), "error");
        $haserror = TRUE;
        break;
      }

      $objectid = $data[0];
      $filedsid = $data[1];
      $filepath = NULL;
      if (isset($data[2])) {
        $filepath = trim($data[2]);
        if (!empty($filepath)) {
          if (!is_file($filepath)) {
            drush_log(t("File @filepath in CSV does not exist.", array("@filepath" => $filepath)), 'error');
            $haserror = TRUE;
            break;
          }
        }
      }

      $objects = find_fedora_object_ids($objectid);

      if (count($objects) == 0) {
        drush_log(t("Found no object for id @objectid", array("@objectid" => $objectid)), 'error');
        $haserror = TRUE;
        break;
      }
      if (count($objects) > 1) {
        drush_log(t("Found multiple objects for id @objectid", array("@objectid" => $objectid)), 'error');
        $haserror = TRUE;
        break;
      }

      if (empty($filedsid)) {
        drush_log(t("Found no DSID for id @objectid", array("@objectid" => $objectid)), 'error');
        $haserror = TRUE;
        break;
      }
      if (array_key_exists($objectid, $ingest_objects) && array_key_exists($filedsid, $ingest_objects[$objectid])) {
        drush_log(t("Object with id @objectid already has a DSID of type @dsid", array("@objectid" => $objectid, "@dsid" => $filedsid)), 'warning');
      }

      if (array_key_exists($objectid, $ingest_objects) !== TRUE) {
        $ingest_objects[$objectid] = array();
      }
      $ingest_objects[$objectid][$filedsid] = array();

      $ingest_objects[$objectid][$filedsid]["object"] = islandora_object_load($objects[0]);
      $ingest_objects[$objectid][$filedsid]["filepath"] = $filepath;
    }

    if (!$haserror) {
      // TODO: looks alot like ubl_batch_ingest_datastreams_confirm_form_submit. Maybe put this in one function
      foreach ($ingest_objects as $objectid => $dsids) {
        foreach ($dsids as $dsid => $info) {
          $object = $info['object'];
          if (isset($info['filepath']) && $info['filepath'] !== NULL && strlen($info['filepath']) > 0) {
            drush_log(t('Ingesting datastream @ds into object @objectid', array('@ds' => $dsid, '@objectid' => $objectid)), 'notice');
	    $filepath = $info['filepath'];
	    $ingest = !isset($object[$dsid]);
	    if ($ingest) {
	      $mimetype = file_get_mimetype($filepath);
	      $ds = $object->constructDatastream($dsid, 'M');
	      $ds->label = basename($filepath);
	      $ds->mimeType = $mimetype;
	    }
	    else {
	      $ds = $object[$dsid];
	    }
	    $ds->setContentFromFile($filepath);
	    if ($ingest) {
	      $object->ingestDatastream($ds);
	    }
	  }
	  else {
	    // Generate a new datastream.
            drush_log(t('Generating datastream @ds for object @objectid', array('@ds' => $dsid, '@objectid' => $objectid)), 'notice');
	    if ($dsid === 'JPG') {
	      if (module_load_include('inc', 'islandora_large_image', 'includes/derivatives') !== FALSE) {
		islandora_large_image_create_jpg_derivative($object, FALSE);
              }
	    }
	    elseif ($dsid === 'JP2') {
	      if (module_load_include('inc', 'islandora_large_image', 'includes/derivatives') !== FALSE) {
		islandora_large_image_create_jp2_derivative($object, FALSE);
              }
	    }
	    elseif ($dsid === 'TN') {
	      if (module_load_include('inc', 'islandora_large_image', 'includes/derivatives') !== FALSE) {
		islandora_large_image_create_tn_derivative($object, FALSE);
              }
	    }
	    elseif ($dsid === 'PDF') {
	      if (module_load_include('inc', 'islandora_solution_pack_pdf', 'includes/derivatives') !== FALSE) {
	        islandora_pdf_add_pdfa_derivative($object, FALSE);
              }
	    }
	    elseif ($dsid === 'OCR') {
	      if (module_load_include('inc', 'islandora_ocr', 'includes/derivatives') !== FALSE) {
		islandora_ocr_derive_ocr($object, FALSE);
              }
	    }
	    elseif ($dsid === 'HOCR') {
	      if (module_load_include('inc', 'islandora_ocr', 'includes/derivatives') !== FALSE) {
	        islandora_ocr_derive_hocr($object, FALSE);
              }
	    }
	    elseif ($dsid === 'HDL') {
	      if (module_load_include('inc', 'islandora_handle', 'includes/derivatives') !== FALSE) {
		$result = islandora_handle_append_handles_from_configuration($object);
		if (isset($result) && $result['success'] === TRUE) {
		  islandora_handle_add_to_dc_datastream($object);
                }
	      }
	    }
	    elseif ($dsid === 'DIMENSIONS') {
	      if ($object['JP2']) {
		if (module_load_include('inc', 'islandora_paged_content', 'includes/utilities') !== FALSE) {
		  islandora_paged_content_add_dimensions_relationships($object);
                }
	      }
	    }
	  }
        }
      }
    }
    fclose($fh);
  }
  else {
    drush_log(t('Cannot open file @file', array('@file' => $csvfile)), 'error');
  }
}
