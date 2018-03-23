<?php
/**
 * @file
 * Utilities.
 *
 *
 *  Copyright 2017 Leiden University Library
 *
 *  This file is part of islandora_batch_modify.
 *
 *  islandora_batch_modify is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

function islandora_batch_modify_parse_csv_file($csvfile, $logfunction) {
  ini_set("auto_detect_line_endings", "1");
  $delimit = "\t";
  $result = array();
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

        $foundobject = find_fedora_object_ids($objectid, TRUE);

        if (count($foundobject) != 1) {
          // This is a header, so skip it.
          continue;
        }
      }

      if (count($data) == 1 && trim($data[0]) === "") {
        // Just skip an empty.
        continue;
      }
      if (count($data) != 3 && count($data) != 2) {
        $logfunction(t("Line @linenr has @countcolumn columns, but should be 3",
          array(
            "@linenr" => $linenr,
            "@countcolumn" => count($data),
          )), "error");
        $haserror = TRUE;
        break;
      }

      $objectid = $data[0];
      $modifywhat = $data[1];
      $modifyvalue = NULL;
      if (isset($data[2])) {
        $modifyvalue = trim($data[2]);
      }

      $objects = find_fedora_object_ids($objectid, TRUE);

      if (count($objects) == 0) {
        $logfunction(t("Found no object for id @objectid", array("@objectid" => $objectid)), 'error');
        //$haserror = TRUE;
        //break;
      }
      if (count($objects) > 1) {
        $logfunction(t("Found multiple objects for id @objectid", array("@objectid" => $objectid)), 'error');
        //$haserror = TRUE;
        //break;
      }

      if (empty($modifywhat)) {
        $logfunction(t("Found no DSID or property to modify for id @objectid", array("@objectid" => $objectid)), 'error');
        //$haserror = TRUE;
        //break;
      }
      if (isset($result[$objectid][$modifywhat])) {
        $logfunction(t("Multiple modifcations on same DSID or property '@modifywhat' for id @objectid, only last one will be done.", array("@objectid" => $objectid, "@modifywhat" => $modifywhat)), 'warning');
      }

      if (!isset($result[$objectid])) {
        $result[$objectid] = array();
      }
      $result[$objectid][$modifywhat] = array();

      if ($modifywhat === 'add_to_collection') {
        $result[$objectid][$modifywhat]["is_about"] = 'collection';
      }
      else if ($modifywhat === 'remove_from_collection') {
        $result[$objectid][$modifywhat]["is_about"] = 'collection';
      }
      else if (substr($modifywhat, 0, strlen('property')) === 'property') {
        $result[$objectid][$modifywhat]["is_about"] = 'property';
      }
      else if (preg_match("/^[A-Z0-9_-]+$/", $modifywhat)) {
        $result[$objectid][$modifywhat]["is_about"] = 'datastream';
      }
      else {
        $result[$objectid][$modifywhat]["is_about"] = 'dontknow';
      }

      if (isset($modifyvalue) && strlen($modifyvalue) > 0) {
        $result[$objectid][$modifywhat]["value"] = $modifyvalue;
      }
    }
  }
  fclose($fh);
  if ($haserror) {
    return FALSE;
  }
  return $result;
}

function islandora_batch_modify_do_work($workinfo, $logfunction, $loglevel = 'error') {
  foreach ($workinfo as $objectid => $modifywhats) {
    $object = islandora_object_load($objectid);
    if (!$object) {
      if ($loglevel === 'notice' || $loglevel === 'error') {
        $logfunction(t('Cannot find object @objectid', array('@objectid' => $objectid)), 'error');
      }
      continue;
    }
    foreach ($modifywhats as $modifywhat => $modifyinfo) {
      islandora_batch_modify_do_work_for_one($object, $modifywhat, $modifyinfo, $logfunction, $loglevel = 'error');
    }
  }
}

function islandora_batch_modify_do_work_for_one($object, $modifywhat, $modifyinfo, $logfunction, $loglevel = 'error') {
  $objectid = $object->id;
  if (isset($modifyinfo['value']) && strpos($modifyinfo['value'], 'generate') !== 0) {
    $value = $modifyinfo['value'];
    if ($modifywhat === 'add_to_collection') {
      $isadded = FALSE;
      $collection = islandora_object_load($value);
      if ($collection && in_array('islandora:collectionCModel', $collection->models)) {
        if (module_load_include('inc', 'islandora_basic_collection', 'includes/utilities')) {
          islandora_basic_collection_add_to_collection($object, $collection); 
          $parentids = islandora_basic_collection_get_parent_pids($object);
          if (in_array($value, $parentids)) {
            $isadded = TRUE;
          }
        }
      }
      if ($isadded) {
        if ($loglevel === 'notice') {
          $logfunction(t('Successfully added object @objectid to collection @value', array('@value' => $value, '@objectid' => $objectid)), 'notice');
        }
      }
      else {
        if ($loglevel === 'notice' || $loglevel === 'error') {
          $logfunction(t('Failed to add object @objectid to collection @value', array('@value' => $value, '@objectid' => $objectid)), 'error');
        }
      }
    }
    else if ($modifywhat === 'remove_from_collection') {
      $isremoved = FALSE;
      $collection = islandora_object_load($value);
      if ($collection && in_array('islandora:collectionCModel', $collection->models)) {
        if (module_load_include('inc', 'islandora_basic_collection', 'includes/utilities')) {
          $beforeparentids = islandora_basic_collection_get_parent_pids($object);
          if (in_array($value, $beforeparentids)) {
            islandora_basic_collection_remove_from_collection($object, $collection); 
            $parentids = islandora_basic_collection_get_parent_pids($object);
            if (!in_array($value, $parentids)) {
              $isremoved = TRUE;
            }
          }
        }
      }
      if ($isremoved) {
        if ($loglevel === 'notice') {
          $logfunction(t('Successfully removed object @objectid from collection @value', array('@value' => $value, '@objectid' => $objectid)), 'notice');
        }
      }
      else if (in_array($value, $beforeparentids)) {
        if ($loglevel === 'notice' || $loglevel === 'error') {
          $logfunction(t('Failed to remove object @objectid from collection @value', array('@value' => $value, '@objectid' => $objectid)), 'error');
        }
      }
    }
    else if (substr($modifywhat, 0, strlen('property')) === 'property') {
      $isgoodproperty = FALSE;
      $parts = explode(':', $modifywhat);
      if (count($parts) === 2) {
        if ($parts[1] === 'label') {
          try {
            $isgoodproperty = TRUE;
            $object->label = $value;
          }
          catch (Exception $e) {
            if ($loglevel === 'notice' || $loglevel === 'error') {
              $logfunction(t('Error updating label of object @objectid: @s', array('@s' => $e->getMessage(), '@objectid' => $objectid)), 'error');
            }
          }
        }
        else if ($parts[1] === 'state') {
          if (in_array($value, array('A', 'I', 'D'))) {
            try {
              $isgoodproperty = TRUE;
              $object->state = $value;
            }
            catch (Exception $e) {
              if ($loglevel === 'notice' || $loglevel === 'error') {
                $logfunction(t('Error updating state of object @objectid: @s', array('@s' => $e->getMessage(), '@objectid' => $objectid)), 'error');
              }
            }
          }
        }
        else if ($parts[1] === 'owner') {
          try {
            $isgoodproperty = TRUE;
            $object->owner = $value;
          }
          catch (Exception $e) {
            if ($loglevel === 'notice' || $loglevel === 'error') {
              $logfunction(t('Error updating owner of object @objectid: @s', array('@s' => $e->getMessage(), '@objectid' => $objectid)), 'error');
            }
          }
        }
      }
      else if (count($parts) === 3) {
        $dsid = $parts[1];
        $what = $parts[2];
        if (isset($object[$dsid])) {
          if ($what === 'label') {
            try {
              $isgoodproperty = TRUE;
              $object[$dsid]->label = $value;
            }
            catch (Exception $e) {
              if ($loglevel === 'notice' || $loglevel === 'error') {
                $logfunction(t('Error updating label of datastream of object @objectid: @s', array('@s' => $e->getMessage(), '@objectid' => $objectid)), 'error');
              }
            }
          }
          else if ($what === 'state') {
            if (in_array($value, array('D'))) {
              // can only delete a datastream  for now...
              try {
                $isgoodproperty = TRUE;
                islandora_delete_datastream($object[$dsid]);
              }
              catch (Exception $e) {
                if ($loglevel === 'notice' || $loglevel === 'error') {
                  $logfunction(t('Error updating label of datastream of object @objectid: @s', array('@s' => $e->getMessage(), '@objectid' => $objectid)), 'error');
                }
              }
            }
          }
        }
        else {
          if ($loglevel === 'notice' || $loglevel === 'error') {
            $logfunction(t('Datastream @ds for object @objectid does not exist.', array('@ds' => $dsid, '@objectid' => $objectid)), 'error');
          }
        }
      }
      if ($isgoodproperty) {
        if ($loglevel === 'notice') {
          $logfunction(t('Successfully changed "@what" to "@value" of object @objectid', array('@what' => $modifywhat, '@value' => $value, '@objectid' => $objectid)), 'notice');
        }
      }
      else {
        if ($loglevel === 'notice' || $loglevel === 'error') {
          $logfunction(t('Property @prop is not valid', array('@prop' => $modifywhat)), 'error');
        }
      }
    }
    else if (preg_match("/^[A-Z0-9_-]+$/", $modifywhat)) {
      $dsid = $modifywhat;
      // Allow modification of "normal" datastream ids, but dont allow POLICY, RELS-EXT or RELS-INT.
      if (in_array($dsid, array('POLICY', 'RELS-EXT', 'RELS-INT'))) {
        if ($loglevel === 'notice' || $loglevel === 'error') {
          $logfunction(t('Modification of datastream @ds of object @objectid is not allowed', array('@ds' => $dsid, '@objectid' => $objectid)), 'error');
        }
        return;
      }
      $filepath = $value;
      if ($loglevel === 'notice') {
        $logfunction(t('Ingesting file @file into datastream @ds of object @objectid', array('@file' => $filepath, '@ds' => $dsid, '@objectid' => $objectid)), 'notice');
      }
      if (!is_file($filepath)) {
        if ($loglevel === 'notice' || $loglevel === 'error') {
          $logfunction(t('Cannot find file @filepath for ingesting into @ds of object @objectid', array('@filepath' => $filepath, '@ds' => $dsid, '@objectid' => $objectid)), 'error');
        }
        return;
      }
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
      if ($loglevel === 'notice' || $loglevel === 'error') {
        $logfunction(t('Modification of @what of object @objectid is not possible, because it is not a valid datastream or property.', array('@what' => $modifywhat, '@objectid' => $objectid)), 'error');
      }
    }
  }
  else {
    // Generate a new datastream.
    $options = array();
    if (isset($modifyinfo['value'])) {
      $jsonoptions = preg_replace('/^(?:re)?generate[\s:=]*|\s+$/', '', $modifyinfo['value']);
      if (strlen($jsonoptions) > 0) {
        $options = json_decode($jsonoptions, TRUE);
        if ($options === NULL) {
          if ($loglevel === 'notice' || $loglevel === 'error') {
            $logfunction(t('Skipping generation of @ds of object @objectid, because the generation options ("!options") are not valid JSON.',
              array('@ds' => $modifywhat, '@objectid' => $objectid, '!options' => $jsonoptions)), 'error');
          }
          return;
        }
      }
    }
    if (!preg_match("/^[A-Z0-9_-]+$/", $modifywhat)) {
      if ($loglevel === 'notice' || $loglevel === 'error') {
        $logfunction(t('Skipping generation of @ds of object @objectid, because @ds is not a valid datastream.', array('@ds' => $modifywhat, '@objectid' => $objectid)), 'error');
      }
      return;
    }
    $dsid = $modifywhat;
    if (isset($object[$dsid])) {
      if ($loglevel === 'notice') {
        $logfunction(t('Skipping generation of @ds of object @objectid, because it already exists.', array('@ds' => $dsid, '@objectid' => $objectid)), 'notice');
      }
      return;
    }
    if ($loglevel === 'notice') {
      $logfunction(t('Generating datastream @ds for object @objectid', array('@ds' => $dsid, '@objectid' => $objectid)), 'notice');
    }
    if ($dsid === 'HDL') {
      if (module_load_include('inc', 'islandora_handle', 'includes/derivatives') !== FALSE) {
        $options += array('destination_dsid' => 'MODS');
        if (function_exists('islandora_handle_append_handle_from_configuration')) {
          $result = islandora_handle_append_handle_from_configuration($object, FALSE, $options);
        }
        else if (function_exists('islandora_handle_append_handles_from_configuration')) {
          $result = islandora_handle_append_handles_from_configuration($object, FALSE, $options);
        }
        if (isset($result) && $result['success'] === TRUE) {
          islandora_handle_add_to_dc_datastream($object);
          $logfunction(t('Handle created succesfully'), 'notice');
        }
        else {
          display_log($result, $logfunction, 'error');
        }
      }
      else {
        $logfunction(t('Islandora_handle module is not enabled'), 'error');
      }
    }
    elseif ($dsid === 'DIMENSIONS') {
      if ($object['JP2']) {
        if (module_load_include('inc', 'islandora_paged_content', 'includes/utilities') !== FALSE) {
          islandora_paged_content_add_dimensions_relationships($object);
          $logfunction(t('Dimensions created succesfully'), 'notice');
        }
        else {
          $logfunction(t('Islandora_paged_content module is not enabled'), 'error');
        }
      }
      else {
        $logfunction(t('Object @objectid does not have a JP2 datastream, so dimensions cannot be generated', array('@objectid' => $object->id)), 'error');
      }
    }
    elseif ($dsid === 'PDF' && in_array('islandora:bookCModel', $object->models)) {
      if (module_load_include('inc', 'islandora_paged_content', 'includes/utilities') && module_load_include('inc', 'islandora_paged_content', 'includes/batch')) {
        $pageids = array_keys(islandora_paged_content_get_pages($object));
        if (isset($modifyinfo['value']) && strpos($modifyinfo['value'], 'regenerate') === 0) {
          $batch = islandora_paged_content_create_pdf_batch($object, $pageids, $options);
          batch_set($batch);
          if ($logfunction === 'drush_log') {
            drush_backend_batch_process();
          }
        }
        else {
          // Create and collect PDF's for pages.
          $pdf_files = array();
          foreach ($pageids as $pageid) {
            $page = islandora_object_load($pageid);
            if (isset($page[$dsid])) {
              $pdf_file = drupal_realpath("temporary://{$page->id}_PDF.pdf");
              if ($page[$dsid]->getContent($pdf_file)) {
                $pdf_files[] = $pdf_file;
              }
              else {
                $logfunction(t('Failed to get existing PDF for page @page', array('@page' => $pageid)), 'error');
              }
            }
            else {
              $pdf_file = islandora_paged_content_page_derive_pdf($page, $options);
              if ($pdf_file) {
                islandora_paged_content_update_datastream($page, $pdf_file, 'PDF');
                $pdf_files[] = $pdf_file;
              }
              else {
                $logfunction(t('Failed to generate PDF for page @page', array('@page' => $pageid)), 'error');
              }
            }
          }

          // Combine the page PDF's into one big PDF
          $pdf_file = drupal_realpath("temporary://{$object->id}_PDF.pdf");
          islandora_paged_content_pdf_combine($pdf_files, $pdf_file);
          islandora_paged_content_update_datastream($object, $pdf_file, 'PDF');

          // Clean up temporary files.
          file_unmanaged_delete($pdf_file);
          foreach ($pdf_files as $pdf_file) {
            file_unmanaged_delete($pdf_file);
          }
        }
      }
      else {
        $logfunction(t('Islandora_paged_content module is not enabled'), 'error');
      }
    }
    elseif ($dsid === 'PDF' && in_array('islandora:pageCModel', $object->models)) {
      if (module_load_include('inc', 'islandora_paged_content', 'includes/utilities')) {
        if (islandora_paged_content_page_derive_pdf_datastream($object, $options)) {
          $logfunction(t('PDF created succesfully'), 'notice');
        }
        else {
          $logfunction(t('Failed to created PDF of page.'), 'error');
        }
      }
      else {
        $logfunction(t('Islandora_paged_content module is not enabled'), 'error');
      }
    }
    else {
      $options += array( 'destination_dsid' => $dsid);
      module_load_include('inc', 'islandora', 'includes/derivatives');
      $logging = islandora_do_derivatives($object, $options);
      if ($loglevel === 'notice' || $loglevel === 'error') {
        if (count($logging) === 0) {
          // There is no derivative generation function for this dsid.
          $logfunction(t('Generation of @ds of object @objectid is not possible, because no derivative function is defined for "@ds" for objects with content models "@cmodel".',
            array('@ds' => $dsid, '@objectid' => $objectid, '@cmodel' => implode(',', $object->models))), 'error');
        }
        else {
          foreach ($logging as $log) {
            if (isset($log['success']) && $log['success']) {
              display_log($log, $logfunction, 'notice');
            }
            else {
              display_log($log, $logfunction, 'error');
            }
          }
        }
      }
    }
  }
}

function display_log($log, $logfunction, $logtype) {
  if (isset($log['messages'])) {
    foreach ($log['messages'] as $message) {
      if (isset($message['message'])) {
        if (isset($message['message_sub'])) {
          $logfunction(t($message['message'], $message['message_sub']), $logtype);
        }
        else {
          $logfunction($message['message'], $logtype);
        }
      }
      else {
        $logfunction(t('Unknown message: @desc', array('@desc' => drupal_json_encode($message))), $logtype);
      }
    }
  }
  else {
    $logfunction(t('Unknown message: @desc', array('@desc' => drupal_json_encode($log))), $logtype);
  }
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

  $limit = 100;
  $qp->buildAndExecuteQuery($search, array('limit' => $limit), FALSE);

  $r = $qp->islandoraSolrResult;

  $numfound = min($r['response']['numFound'], $limit);

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

