<?php

/**
 * @file
 * Handles the ingest of datastreams from a zip file.
 */

/**
 * The form for handling the ingest of datastreams.
 *
 * @param array $form
 *   The Drupal form definition.
 * @param array $form_state
 *   The Drupal form state.
 */
function ubl_batch_ingest_datastreams_import_form(array $form, array &$form_state) {
  if (isset($_SESSION['ubl_batch_ingest_datastreams_ingest_objects'])) {
    return ubl_batch_ingest_datastreams_confirm_form($form, $form_state);
  }
  else {
    return ubl_batch_ingest_datastreams_import_zip_or_csv_form($form, $form_state);
  }
}

/**
 * The form for handling the submit of ingest of datastreams.
 *
 * @param array $form
 *   The Drupal form definition.
 * @param array $form_state
 *   The Drupal form state.
 */
function ubl_batch_ingest_datastreams_import_form_submit(array $form, array &$form_state) {
  if (isset($_SESSION['ubl_batch_ingest_datastreams_ingest_objects'])) {
    return ubl_batch_ingest_datastreams_confirm_form_submit($form, $form_state);
  }
  else {
    return ubl_batch_ingest_datastreams_import_zip_or_csv_form_submit($form, $form_state);
  }
}

/**
 * The form for uploading zip or csv.
 *
 * @param array $form
 *   The Drupal form definition.
 * @param array $form_state
 *   The Drupal form state.
 */
function ubl_batch_ingest_datastreams_import_zip_or_csv_form(array $form, array &$form_state) {
  $extensions = array('zip csv');
  $upload_size = min((int) ini_get('post_max_size'), (int) ini_get('upload_max_filesize'));
  $form['file'] = array(
    '#title' => t('Upload zip or csv'),
    '#type' => 'managed_file',
    '#description' => t('Select a file to upload.<br/>Files must be less than <b>@size MB.</b><br/>Allowed file types: <b>@ext.</b>', array('@size' => $upload_size, '@ext' => $extensions[0])),
    '#upload_location' => 'temporary://',
    '#required' => TRUE,
    '#upload_validators' => array(
      'file_validate_extensions' => $extensions,
      'file_validate_size' => array($upload_size * 1024 * 1024),
    ),
  );
  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Ingest datastreams'),
  );
  return $form;
}

/**
 * Processes uploaded zip.
 *
 * @param array $form
 *   The Drupal form definition.
 * @param array $form_state
 *   The Drupal form state.
 */
function ubl_batch_ingest_datastreams_import_zip_or_csv_form_submit(array $form, array &$form_state) {
  $uploadedfile = file_load($form_state['values']['file']);

  $csvpath = FALSE;
  if ($uploadedfile->filemime === 'application/zip') {
    $zipfile = $uploadedfile;
    $zippath = drupal_realpath($zipfile->uri);

    $zip = new ZipArchive();
    if (($error = $zip->open($zippath)) !== TRUE) {
      drupal_set_message(t('Error opening the provided Zip file.  Code: %code', array(
            '%code' => $error,
          )));
      return;
    }

    $zipdirectory = preg_replace("/\.[a-z0-9]{3}$/i", '_' . $zipfile->timestamp . '/', $zipfile->uri);
    if ($zip->extractTo($zipdirectory) !== TRUE) {
      drupal_set_message(t('Error unzipping the provided Zip file.'));
      return;
    }
    $count = $zip->numFiles;
    $files = array();
    for ($i = 0; $i < $count; $i++) {
      $name = $zip->getNameIndex($i);
      $path_info = pathinfo($name);

      if (strpos($path_info['dirname'], '__MACOSX') === FALSE
        && isset($path_info['extension'])
        && $path_info['extension'] != 'DS_Store') {
        $filename = $path_info['basename'];
        $dirname = $path_info['dirname'];
        $realpath = $zipdirectory . $dirname . '/' . $filename;
        $files[$filename] = $realpath;

        if (isset($path_info["extension"]) && $path_info["extension"] === 'csv') {
          if ($csvpath === FALSE) {
            $csvpath = $realpath;
          }
          else {
            // Multiple CSV files found in zip file.
            // Get the one with the same name as the zip file.
            $newcsvpath = $zipdirectory . preg_replace("/\.[a-z0-9]{3}$/i", '.csv', $zipfile->filename);
            if (file_exists($newcsvpath)) {
              $csvpath = $newcsvpath;
            }
            else {
              $csvpath = NULL;
            }
          }
        }
      }
    }
    $zip->close();
    if (!(isset($csvpath) && file_exists($csvpath))) {
      drupal_set_message(t('Provided Zip file does not contain a valid CSV file.'), 'error');
      return;
    }
  }
  else {
    $csvpath = drupal_realpath($uploadedfile->uri);
  }

  $generatable_dsids = array();
  if (module_exists('islandora_large_image')) {
    $generatable_dsids[] = 'JP2';
    $generatable_dsids[] = 'JPG';
    $generatable_dsids[] = 'TN';
  }
  if (module_exists('islandora_solution_pack_pdf')) {
    $generatable_dsids[] = 'PDF';
  }
  if (module_exists('islandora_ocr')) {
    $generatable_dsids[] = 'OCR';
    $generatable_dsids[] = 'HOCR';
  }
  if (module_exists('islandora_handle')) {
    $generatable_dsids[] = 'HDL';
  }
  if (module_exists('islandora_paged_content')) {
    $generatable_dsids[] = 'DIMENSIONS';
  }

  ini_set("auto_detect_line_endings", "1");
  $delimit = "\t";
  $ingest_objects = array();
  $linenr = 0;
  if (($fh = fopen($csvpath, "r")) !== FALSE) {
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

        $foundobject = find_fedora_object_ids($objectid, TRUE);

        if (count($foundobject) != 1) {
          // This is a header, so skip it.
          continue;
        }
      }

      if (count($data) == 1 && trim($data[0]) === "") {
        continue;
      }
      if (count($data) != 3 && count($data) != 2) {
        drupal_set_message(t("Line @linenr has @countcolumn columns, but should be 3",
          array(
            "@linenr" => $linenr,
            "@countcolumn" => count($data),
          )), "error");
        if (count($data) < 2) {
          // Just skip an empty line.
          continue;
        }
      }

      $objectid = $data[0];
      $filedsid = $data[1];
      $filepath = NULL;
      if (isset($data[2])) {
        $filename = trim($data[2]);
        if (!empty($filename)) {
          if (array_key_exists($filename, $files)) {
            $filepath = $files[$filename];
          }
          else {
            drupal_set_message(t("File @filename in CSV does not exist in ZIP file", array("@filename" => $filename)), 'error');
          }
        }
      }

      $objects = find_fedora_object_ids($objectid, TRUE);

      if (empty($filedsid)) {
        drupal_set_message(t("Found no DSID for id @objectid", array("@objectid" => $objectid)), 'warning');
      }
      if (array_key_exists($objectid, $ingest_objects) && array_key_exists($filedsid, $ingest_objects[$objectid])) {
        drupal_set_message(t("Object with id @objectid already has a DSID of type @dsid", array("@objectid" => $objectid, "@dsid" => $filedsid)), 'warning');
      }

      if (array_key_exists($objectid, $ingest_objects) !== TRUE) {
        $ingest_objects[$objectid] = array();
      }
      $enable = TRUE;
      $ingest_objects[$objectid][$filedsid] = array();

      if (count($objects) == 1) {
        $ingest_objects[$objectid][$filedsid]["object"] = islandora_object_load($objects[0]);
      }
      else {
        $enable = FALSE;
        if (count($objects) > 1) {
          $ingest_objects[$objectid][$filedsid]["objmsg"] = t("multiple");
        }
        else {
          $ingest_objects[$objectid][$filedsid]["objmsg"] = t("none");
        }
        $ingest_objects[$objectid][$filedsid]['disallow'] = TRUE;
      }
      if (file_exists($filepath) === TRUE) {
        $ingest_objects[$objectid][$filedsid]["filename"] = $filename;
        $ingest_objects[$objectid][$filedsid]["filepath"] = $filepath;
      }
      else {
        if (!in_array($filedsid, $generatable_dsids)) {
          $enable = FALSE;
          $ingest_objects[$objectid][$filedsid]['disallow'] = TRUE;
          drupal_set_message(t("Cannot generate @dsid for @objectid", array("@dsid" => $filedsid, "@objectid" => $objectid)), "warning");
        }
      }
      if (empty($filedsid)) {
        $enable = FALSE;
      }
      $ingest_objects[$objectid][$filedsid]["enabled"] = $enable;
    }
    fclose($fh);
  }

  $_SESSION['ubl_batch_ingest_datastreams_ingest_objects'] = $ingest_objects;
  $_SESSION['ubl_batch_ingest_datastreams_finished'] = FALSE;
}

/**
 * The form for confirming the ingest datastreams.
 */
function ubl_batch_ingest_datastreams_confirm_form(array $form, array &$form_state) {
  $ingest_objects = $_SESSION['ubl_batch_ingest_datastreams_ingest_objects'];
  $options = array();
  $defaultvalues = array();
  if (count($form_state["build_info"]["args"]) == 1) {
    $objectid = $form_state["build_info"]["args"][0];

  }
  else {
    foreach ($ingest_objects as $objectid => $dsids) {
      foreach ($dsids as $dsid => $values) {
        $key = $objectid . '_-_' . $dsid;
        $object = NULL;
        $is_collection = FALSE;
        $options[$key]['objectid'] = check_plain($objectid);
        if (array_key_exists('object', $values)) {
          $object = $values['object'];
          $is_collection = in_array('islandora:collectionCModel', $object->models);
          $options[$key]['object'] = array(
            'data' => array(
              '#type' => 'link',
              '#title' => filter_xss($object->id),
              '#href' => "islandora/object/{$object->id}",
            ),
          );
        }
        else {
          $options[$key]['object'] = $values["objmsg"];
        }
        if (array_key_exists("filename", $values)) {
          $options[$key]['file'] = $values["filename"];
        }
        else {
          $options[$key]['file'] = "-";
        }
        $options[$key]['dsid'] = $dsid;
        if ($is_collection) {
          $whichdesc = t("all @dsid's", array("@dsid" => $dsid));
        }
        else {
          $whichdesc = $dsid;
        }
        if (!isset($object)) {
          $options[$key]['action'] = t('No action');
        }
        elseif (array_key_exists('disallow', $values) && $values['disallow']) {
          $options[$key]['action'] = t('Action not allowed');
        }
        elseif (!$is_collection && isset($object[$dsid])) {
          if (array_key_exists("filename", $values)) {
            $options[$key]['action'] = t('Replace @whichdesc', array('@whichdesc' => $whichdesc));
          }
          else {
            $options[$key]['action'] = t('Regenerate @whichdesc', array('@whichdesc' => $whichdesc));
          }
        }
        else {
          if (array_key_exists("filename", $values)) {
            $options[$key]['action'] = t('Add @whichdesc', array('@whichdesc' => $whichdesc));
          }
          else {
            $options[$key]['action'] = t('Generate @whichdesc', array('@whichdesc' => $whichdesc));
          }
        }
        if (array_key_exists("disallow", $values) && $values['disallow']) {
          $options[$key]['#disabled'] = TRUE;
        }
        $defaultvalues[$key] = $values["enabled"];
      }
    }
    $table = array(
      '#type' => 'tableselect',
      '#header' => array(
        'objectid' => t('Object ID in CSV'),
        'object' => t('Found item'),
        'dsid' => t('DSID in CSV'),
        'file' => t('File in CSV'),
        'action' => t('Action'),
      ),
      '#options' => $options,
      '#default_value' => $defaultvalues,
      '#empty' => t('No data available.'),
      '#disabled' => ($_SESSION['ubl_batch_ingest_datastreams_finished']),
    );
    $form['table'] = $table;

    $form['cancel'] = array(
      '#type' => 'submit',
      '#value' => t('Back to upload'),
    );
    if (!$_SESSION['ubl_batch_ingest_datastreams_finished']) {
      $form['submit'] = array(
        '#type' => 'submit',
        '#value' => t('Ingest datastreams'),
      );
    }
  }

  return $form;
}

/**
 * Submitting the form for confirming the ingest of datastreams.
 */
function ubl_batch_ingest_datastreams_confirm_form_submit(array $form, array &$form_state) {
  if ($form_state['clicked_button']['#id'] === 'edit-cancel') {
    unset($_SESSION['ubl_batch_ingest_datastreams_ingest_objects']);
    unset($_SESSION['ubl_batch_ingest_datastreams_finished']);

    return;
  }
  if ($_SESSION['ubl_batch_ingest_datastreams_finished']) {
    // Do not change anything if already finished.
    return;
  }
  $operations = array();

  foreach ($form_state["values"]["table"] as $key => $isneeded) {
    $parts = explode("_-_", $key);
    $objectid = $parts[0];
    $dsid = $parts[1];
    if ($isneeded) {
      $params = $_SESSION['ubl_batch_ingest_datastreams_ingest_objects'][$objectid][$dsid];
      $params['dsid'] = $dsid;
      $_SESSION['ubl_batch_ingest_datastreams_ingest_objects'][$objectid][$dsid]['enabled'] = TRUE;

      $object = $params['object'];
      $is_collection = in_array('islandora:collectionCModel', $object->models);

      if ($is_collection) {
        list($count, $colobjs) = islandora_basic_collection_get_member_objects($object, -1, -1);

        foreach ($colobjs as $colobj) {
          $objincol = islandora_object_load($colobj['object']['value']);
          if ($objincol) {
            $newparams = $params;
            $newparams["object"] = $objincol;
            $operations[] = array('ubl_batch_ingest_datastreams_operation', array($newparams));
          }
        }
      }
      else {
        $operations[] = array('ubl_batch_ingest_datastreams_operation', array($params));
      }
    }
  }
  if (count($operations) > 0) {
    $batch = array(
      'operations' => $operations,
      'title' => t("Ingesting datastreams"),
      'init_message' => t("Preparing ..."),
      'progress_message' => t('Time elapsed: @elapsed <br/>Estimated time remaining @estimate.'),
      'error_message' => t('An error has occurred.'),
      'file' => drupal_get_path('module', 'ubl_batch_ingest_datastreams') . '/includes/ubl_batch_ingest_datastreams_zip_import.form.inc',
      'finished' => 'ubl_batch_ingest_datastreams_operation_finished',
    );
    batch_set($batch);
  }
}

/**
 * Process the ingestion of datastreams per object.
 */
function ubl_batch_ingest_datastreams_operation($params, &$context) {
  if (array_key_exists('object', $params)) {
    $object = $params['object'];
    $dsid = $params['dsid'];

    if (array_key_exists('filepath', $params)) {
      $filepath = $params['filepath'];
      $ingest = !isset($object[$dsid]);
      if ($ingest) {
        $mimetype = file_get_mimetype($filepath);
        $ds = $object->constructDatastream($dsid, 'M');
        $ds->label = $params['filename'];
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
      if ($dsid === 'JPG') {
        module_load_include('inc', 'islandora_large_image', 'includes/derivatives');
        islandora_large_image_create_jpg_derivative($object, FALSE);
      }
      elseif ($dsid === 'JP2') {
        module_load_include('inc', 'islandora_large_image', 'includes/derivatives');
        islandora_large_image_create_jp2_derivative($object, FALSE);
      }
      elseif ($dsid === 'TN') {
        module_load_include('inc', 'islandora_large_image', 'includes/derivatives');
        islandora_large_image_create_tn_derivative($object, FALSE);
      }
      elseif ($dsid === 'PDF') {
        module_load_include('inc', 'islandora_solution_pack_pdf', 'includes/derivatives');
        islandora_pdf_add_pdfa_derivative($object, FALSE);
      }
      elseif ($dsid === 'OCR') {
        module_load_include('inc', 'islandora_ocr', 'includes/derivatives');
        islandora_ocr_derive_ocr($object, FALSE);
      }
      elseif ($dsid === 'HOCR') {
        module_load_include('inc', 'islandora_ocr', 'includes/derivatives');
        islandora_ocr_derive_hocr($object, FALSE);
      }
      elseif ($dsid === 'HDL') {
        module_load_include('inc', 'islandora_handle', 'includes/derivatives');
        $result = islandora_handle_append_handles_from_configuration($object);
        if (isset($result) && $result['success'] === TRUE) {
          islandora_handle_add_to_dc_datastream($object);
        }
      }
      elseif ($dsid === 'DIMENSIONS') {
        if ($object['JP2']) {
          module_load_include('inc', 'islandora_paged_content', 'includes/utilities');
          islandora_paged_content_add_dimensions_relationships($object);
        }
      }
    }
  }
}

/**
 * Finish process the ingestion of datastreams per object.
 */
function ubl_batch_ingest_datastreams_operation_finished() {
  $_SESSION['ubl_batch_ingest_datastreams_finished'] = TRUE;
}

/**
 * Finding the Fedora object IDs of objects by identifier.
 */
function find_fedora_object_ids($identifier, $exactmatch = FALSE, $query_field = FALSE) {
  module_load_include('inc', 'islandora', 'includes/utilities');
  module_load_include('inc', 'islandora_solr', 'includes/query_processor');
  module_load_include('inc', 'islandora_solr', 'includes/utilities');

  static $alreadyfound = array();

  if (islandora_is_valid_pid($identifier)) {
    // This looks like a islandora object id.
    if (array_key_exists($identifier, $alreadyfound)) {
      return $alreadyfound[$identifier];
    }

    $obj = islandora_object_load($identifier);

    if ($obj) {
      $alreadyfound[$identifier] = array($identifier);

      return $alreadyfound[$identifier];
    }
  }
  static $qp = NULL;
  static $qpc = 0;
  if (is_null($qp) || $qpc > 10) {
    // Get a fresh copy the first time and after each 10 queries.
    // Apparently there are limitations on the amount of queries it can handle.
    $qp = new IslandoraSolrQueryProcessor();
    $qpc = 0;
  }
  $qpc++;

  $escapedid = islandora_solr_lesser_escape($identifier);
  if ($query_field) {
    $search = "$query_field:$escapedid";
  }
  else {
    $search = "catch_all_fields_mt:$escapedid";
  }

  if (array_key_exists($search, $alreadyfound)) {
    return $alreadyfound[$search];
  }

  $qp->buildAndExecuteQuery($search);

  $r = $qp->islandoraSolrResult;

  $numfound = $r['response']['numFound'];

  $objectids = array();
  for ($i = 0; $i < $numfound; $i++) {
    $objid = $r['response']['objects'][$i]['PID'];

    if ($exactmatch) {
      $hasexactmatch = FALSE;
      $solrdoc = $r['response']['objects'][$i]['solr_doc'];
      if ($query_field) {
        if ($solrdoc[$query_field] === $identifier) {
          $hasexactmatch = TRUE;
        }
      }
      else {
        foreach ($solrdoc as $solrkey => $solrval) {
          if (is_array($solrval)) {
            foreach ($solrval as $sv) {
              if ($sv === $identifier) {
                $hasexactmatch = TRUE;
                break 2;
              }
            }
          }
          else {
            if ($solrval === $identifier) {
              $hasexactmatch = TRUE;
              break;
            }
          }
        }
      }
      if (!$hasexactmatch) {
        continue;
      }
    }

    if ($objid) {
      $objectids[] = $objid;
      if (count($objectids) > 1) {
        // Limit the number of object ids found to 2, cause we don't need more.
        break;
      }
    }
  }

  $alreadyfound[$search] = $objectids;

  return $objectids;
}
