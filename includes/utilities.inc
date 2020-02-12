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
 *  islandora_batch_modify is free software: you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as published by
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
 */

/**
 * Parse a CSV file.
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
      }
      if (count($objects) > 1) {
        $logfunction(t("Found multiple objects for id @objectid", array("@objectid" => $objectid)), 'error');
      }

      if (empty($modifywhat)) {
        $logfunction(t("Found no DSID or property to modify for id @objectid", array("@objectid" => $objectid)), 'error');
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
      elseif ($modifywhat === 'remove_from_collection') {
        $result[$objectid][$modifywhat]["is_about"] = 'collection';
      }
      elseif (substr($modifywhat, 0, strlen('property')) === 'property') {
        $result[$objectid][$modifywhat]["is_about"] = 'property';
      }
      elseif (preg_match("/^[A-Z0-9_-]+$/", $modifywhat)) {
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

/**
 * Do the batch modify work for multiple objects.
 */
function islandora_batch_modify_do_work($workinfo, $logfunction, $loglevel = 'error', $skipmodifiedafter = NULL) {
  foreach ($workinfo as $objectid => $modifywhats) {
    $object = islandora_object_load($objectid);
    if (!$object) {
      if ($loglevel === 'notice' || $loglevel === 'error') {
        $logfunction(t('Cannot find object @objectid', array('@objectid' => $objectid)), 'error');
      }
      continue;
    }
    foreach ($modifywhats as $modifywhat => $modifyinfo) {
      islandora_batch_modify_do_work_for_one($object, $modifywhat, $modifyinfo, $logfunction, $loglevel, $skipmodifiedafter);
    }
  }
}

/**
 * Do the batch modify work for one object.
 */
function islandora_batch_modify_do_work_for_one($object, $modifywhat, $modifyinfo, $logfunction, $loglevel = 'error', $skipmodifiedafter = NULL) {
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
    elseif ($modifywhat === 'remove_from_collection') {
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
      elseif (in_array($value, $beforeparentids)) {
        if ($loglevel === 'notice' || $loglevel === 'error') {
          $logfunction(t('Failed to remove object @objectid from collection @value', array('@value' => $value, '@objectid' => $objectid)), 'error');
        }
      }
    }
    elseif (substr($modifywhat, 0, strlen('property')) === 'property') {
      $isgoodproperty = FALSE;
      $parts = explode(':', $modifywhat);
      if (count($parts) === 2) {
        if ($parts[1] === 'label') {
          if (isset($skipmodifiedafter)) {
            $moddate = $object->lastModifiedDate->format("Y-m-d\TH:i:s");
            if (strcmp($skipmodifiedafter, $moddate) < 0) {
              if ($loglevel === 'notice') {
                $logfunction(t('Skipping update of label of object @objid because modification date of object (@moddate) is after @skipmoddate',
                  array(
                    '@objid' => $object->id,
                    '@moddate' => $moddate,
                    '@skipmoddate' => $skipmodifiedafter,
                  )), 'notice');
              }
              return;
            }
          }
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
        elseif ($parts[1] === 'state') {
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
        elseif ($parts[1] === 'owner') {
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
      elseif (count($parts) === 3) {
        $dsid = $parts[1];
        $what = $parts[2];
        if (isset($object[$dsid])) {
          if ($what === 'label') {
            if (isset($skipmodifiedafter)) {
              $moddate = $object[$dsid]->createdDate->format("Y-m-d\TH:i:s");
              if (strcmp($skipmodifiedafter, $moddate) < 0) {
                if ($loglevel === 'notice') {
                  $logfunction(t('Skipping update of label of datastream @dsid of object @objid because modification date of datastream (@moddate) is after @skipmoddate',
                    array(
                      '@dsid' => $dsid,
                      '@objid' => $object->id,
                      '@moddate' => $moddate,
                      '@skipmoddate' => $skipmodifiedafter,
                    )), 'notice');
                }
                return;
              }
            }
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
          elseif ($what === 'state') {
            if (in_array($value, array('D'))) {
              // Can only delete a datastream  for now...
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
          $logfunction(t('Successfully changed "@what" to "@value" of object @objectid', array(
            '@what' => $modifywhat,
            '@value' => $value,
            '@objectid' => $objectid,
          )), 'notice');
        }
      }
      else {
        if ($loglevel === 'notice' || $loglevel === 'error') {
          $logfunction(t('Property @prop is not valid', array('@prop' => $modifywhat)), 'error');
        }
      }
    }
    elseif (preg_match("/^[A-Z0-9_-]+$/", $modifywhat)) {
      $dsid = $modifywhat;
      // Allow modification of "normal" datastream ids, but warn for POLICY,
      // RELS-EXT or RELS-INT.
      if (in_array($dsid, array('POLICY', 'RELS-INT', 'RELS-EXT'))) {
        if ($loglevel === 'notice' || $loglevel === 'error') {
          $logfunction(t('Modification of datastream @ds of object @objectid', array('@ds' => $dsid, '@objectid' => $objectid)), 'warning');
        }
      }
      $controlgroup = (in_array($dsid, array('RELS-INT', 'RELS-EXT')) ? 'X' : 'M');
      if (strpos($value, 'external:') === 0) {
        $controlgroup = 'E';
        $url = substr($value, 9);
        if ($loglevel === 'notice') {
          $logfunction(t('Setting @ds of object @objectid as external to @url', array('@url' => $url, '@ds' => $dsid, '@objectid' => $objectid)), 'notice');
        }
      }
      elseif (strpos($value, 'redirect:') === 0) {
        $controlgroup = 'R';
        $url = substr($value, 9);
        if ($loglevel === 'notice') {
          $logfunction(t('Setting @ds of object @objectid as redirect to @url', array('@url' => $url, '@ds' => $dsid, '@objectid' => $objectid)), 'notice');
        }
      }
      else {
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
      }
      $ingest = !isset($object[$dsid]);
      if (isset($skipmodifiedafter) && !$ingest) {
        $moddate = $object[$dsid]->createdDate->format("Y-m-d\TH:i:s");
        if (strcmp($skipmodifiedafter, $moddate) < 0) {
          if ($loglevel === 'notice') {
            $logfunction(t('Skipping update of datastream @dsid of object @objid because modification date of datastream (@moddate) is after @skipmoddate',
              array('@dsid' => $dsid, '@objid' => $object->id, '@moddate' => $moddate, '@skipmoddate' => $skipmodifiedafter)), 'notice');
          }
          return;
        }
      }

      if ($ingest) {
        $mimetype = file_get_mimetype($filepath);
        $ds = $object->constructDatastream($dsid, $controlgroup);
        $ds->label = basename($filepath);
        $ds->mimeType = $mimetype;
      }
      else {
        $ds = $object[$dsid];
        if ($ds->controlGroup !== $controlgroup) {
          if ($loglevel === 'notice' || $loglevel === 'error') {
            $logfunction(t('Cannot change controlgroup of @ds of object @objectid from @oldcontrolgroup to @controlgroup', array('@controlgroup' => $controlgroup, '@ds' => $dsid, '@objectid' => $objectid, '@oldcontrolgroup' => $ds->controlGroup)), 'error');
          }
          return;
        }
      }
      if ($controlgroup === 'X') {
        $content = file_get_contents($filepath);
        if ($content === FALSE) {
          if ($loglevel === 'notice' || $loglevel === 'error') {
            $logfunction(t('Cannot read file @filename', array('@filename' => $filepath)), 'error');
          }
          return;
        }
        $ds->content = $content;
      }
      elseif ($controlgroup === 'M') {
        $ds->setContentFromFile($filepath);
      }
      else {
        $ds->url = $url;
      }
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
    islandora_batch_modify_generate($object, $dsid, $options, $logfunction, $loglevel);
  }
}

/**
 * Generate a new derivative for an object.
 */
function islandora_batch_modify_generate($object, $dsid, $options = array(), $logfunction = NULL, $loglevel = NULL) {
  module_load_include('inc', 'islandora', 'includes/derivatives');
  module_load_include('inc', 'islandora', 'includes/utilities');

  if ($logfunction === NULL) {
    $logfunction = function($t, $l) {
      // Do nothing.
    };
  }
  $output = array();
  $hooks = islandora_build_hook_list('generate_extra_derivatives', $object->models);
  foreach ($hooks as $hook) {
    $temp = module_invoke_all($hook, $object, $dsid, $options);

    if (!empty($temp)) {
      $output = array_merge_recursive($output, $temp);
    }
  }
  if (!empty($output)) {
    foreach ($output as $log) {
      if (isset($log['success']) && $log['success']) {
        display_log($log, $logfunction, 'notice');
        return TRUE;
      }
      else {
        display_log($log, $logfunction, 'error');
      }
    }
    return FALSE;
  }

  $options += array('destination_dsid' => $dsid);
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
          return TRUE;
        }
        else {
          display_log($log, $logfunction, 'error');
        }
      }
    }
  }
  else {
    foreach ($logging as $log) {
      if (isset($log['success']) && $log['success']) {
        return TRUE;
      }
    }
  }
  return isset($object[$dsid]);
}

function display_log($log, $logfunction, $logtype) {
  if (isset($log['message'])) {
    if (isset($log['message_sub'])) {
      $logfunction(t($log['message'], $log['message_sub']), $logtype);
    }
    else {
      $logfunction($log['message'], $logtype);
    }
  }
  elseif (isset($log['messages'])) {
    foreach ($log['messages'] as $message) {
      display_log($message, $logfunction, $logtype);
    }
  }
  elseif (is_array($log)) {
    foreach ($log as $message) {
      display_log($message, $logfunction, $logtype);
    }
  }
  elseif (is_string($log)) {
    $logfunction($log, $logfunction, $logtype);
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
