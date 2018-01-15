<?php

/**
 * @file
 * Handles the ingest of datastreams from a zip file.
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

/**
 * The form for handling the ingest of datastreams.
 *
 * @param array $form
 *   The Drupal form definition.
 * @param array $form_state
 *   The Drupal form state.
 */
function islandora_batch_modify_import_form(array $form, array &$form_state) {
  if (isset($_SESSION['islandora_batch_modify_ingest_objects'])) {
    return islandora_batch_modify_confirm_form($form, $form_state);
  }
  else {
    return islandora_batch_modify_import_zip_or_csv_form($form, $form_state);
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
function islandora_batch_modify_import_form_submit(array $form, array &$form_state) {
  if (isset($_SESSION['islandora_batch_modify_ingest_objects'])) {
    return islandora_batch_modify_confirm_form_submit($form, $form_state);
  }
  else {
    return islandora_batch_modify_import_zip_or_csv_form_submit($form, $form_state);
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
function islandora_batch_modify_import_zip_or_csv_form(array $form, array &$form_state) {
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
    '#value' => t('Upload file'),
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
function islandora_batch_modify_import_zip_or_csv_form_submit(array $form, array &$form_state) {
  module_load_include('inc', 'islandora_batch_modify', 'includes/utilities');

  $uploadedfile = file_load($form_state['values']['file']);

  $csvpath = FALSE;
  $files = array();
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

  $ingest_objects = islandora_batch_modify_parse_csv_file($csvpath, 'drupal_set_message');

  if ($ingest_objects) {
    $model2generate = array();
    foreach ($ingest_objects as $objectid => $dsid2info) {
      $objects = find_fedora_object_ids($objectid, TRUE);
  
      foreach ($dsid2info as $dsid => $info) {
        $enable = TRUE;
        if (count($objects) == 1) {
          $ingest_objects[$objectid][$dsid]["object"] = islandora_object_load($objects[0]);
        }
        else {
          $enable = FALSE;
          if (count($objects) > 1) {
            $ingest_objects[$objectid][$dsid]["action"] = t("None: multiple objects found");
          }
          else {
            $ingest_objects[$objectid][$dsid]["action"] = t("None: no object found");
          }
        }
        if (empty($dsid)) {
          $ingest_objects[$objectid][$dsid]["action"] = t("None: nothing to do");
          $enable = FALSE;
        }

        $filepath = NULL;
        if ($enable && isset($info['value']) && strlen($info['value']) > 0) {
          if ($info['is_about'] === 'datastream') {
            $filename = $info['value'];
            if (array_key_exists($filename, $files)) {
              $filepath = $files[$filename];
            }
            if ($filepath && file_exists($filepath)) {
              $ingest_objects[$objectid][$dsid]["filename"] = $filename;
              $ingest_objects[$objectid][$dsid]["filepath"] = $filepath;
              $ingest_objects[$objectid][$dsid]["action"] = t('Add @dsid', array('@dsid' => $dsid));
            }
            else {
              $ingest_objects[$objectid][$dsid]["action"] = t('None: file not in ZIP');
              $enable = FALSE;
            }
          }
          else if ($info['is_about'] === 'property') {
            if ((substr($dsid, -6) === ':state') && !in_array($info['value'], array('A', 'I', 'D'))) {
              $ingest_objects[$objectid][$dsid]["action"] = t('None: invalid value');
              $enable = FALSE;
            }
            else {
              $ingest_objects[$objectid][$dsid]["action"] = t('Set value');
            }
          }
          else if ($info['is_about'] === 'collection') {
            $collection = islandora_object_load($info['value']);
            $is_collection = $collection && in_array('islandora:collectionCModel', $collection->models); 
            if ($is_collection) {
              if ($dsid === 'add_to_collection') {
                $ingest_objects[$objectid][$dsid]["action"] = t('Add to collection');
              }
              else if ($dsid === 'remove_from_collection') {
                $ingest_objects[$objectid][$dsid]["action"] = t('Remove from collection');
              }
              else {
                $ingest_objects[$objectid][$dsid]["action"] = t('No action');
                $enable = FALSE;
              }
            }
            else {
              $ingest_objects[$objectid][$dsid]["action"] = t('None: no collection');
              $enable = FALSE;
            }
          }
          else {
            $ingest_objects[$objectid][$dsid]["action"] = t('No action');
            $enable = FALSE;
          }
        }
        else if ($enable) {
          if ($info['is_about'] === 'datastream') {
            if (isset($ingest_objects[$objectid][$dsid]["object"])) {
              $object = $ingest_objects[$objectid][$dsid]["object"];
              $strmodel = implode(',', $object->models);
              module_load_include('inc', 'islandora', 'includes/utilitites');
              if (isset($model2generate[$strmodel][$dsid])) {
                $cangenerate = $model2generate[$strmodel][$dsid];
              }
              else {
                if (isset($model2generate[$strmodel]['_gdsids'])) {
                  $gdsids = $model2generate[$strmodel]['_gdsids'];
                }
                else {
                  $gdsids = islandora_invoke_hook_list(ISLANDORA_DERIVATIVE_CREATION_HOOK, $object->models, array($object));
                  $model2generate[$strmodel]['_gdsids'] = $gdsids;
                }
                $cangenerate = FALSE;
                if ($dsid === 'HDL' || $dsid === 'DIMENSIONS') {
                  $cangenerate = TRUE;
                }
                else {
                  foreach ($gdsids as $gdsid) {
                    if ($gdsid['destination_dsid'] === $dsid) {
                      $cangenerate = TRUE;
                    } 
                  }
                }
                $model2generate[$strmodel][$dsid] = $cangenerate;
              }
              
              if ($cangenerate) {
                if (isset($object[$dsid])) {
                  $ingest_objects[$objectid][$dsid]["action"] = t('None: @dsid exists', array('@dsid' => $dsid));
                  $enable = FALSE;
                }
                else {
                  $ingest_objects[$objectid][$dsid]["action"] = t('Generate @dsid', array('@dsid' => $dsid));
                }
              }
              else {
                $ingest_objects[$objectid][$dsid]["action"] = t('None: cannot generate');
                $enable = FALSE;
              }
            }
          }
          else {
            $ingest_objects[$objectid][$dsid]["action"] = t('None: no value');
            $enable = FALSE;
          }
        }
        $ingest_objects[$objectid][$dsid]["hasaction"] = $enable;
      }
    }
  } 
  $_SESSION['islandora_batch_modify_ingest_objects'] = $ingest_objects;
  $_SESSION['islandora_batch_modify_finished'] = FALSE;
}

/**
 * The form for confirming the ingest datastreams.
 */
function islandora_batch_modify_confirm_form(array $form, array &$form_state) {
  $ingest_objects = $_SESSION['islandora_batch_modify_ingest_objects'];
  $options = array();
  $defaultvalues = array();
  if (count($form_state["build_info"]["args"]) == 1) {
    $objectid = $form_state["build_info"]["args"][0];

  }
  else {
    $options = array();
    $defaultvalues = array();
    $table = array(
      '#type' => 'tableselect',
      '#header' => array(
        'objectid' => t('Object ID'),
        'what' => t('What'),
        'value' => t('Value'),
        'action' => t('Action'),
      ),
      '#options' => $options,
      '#default_value' => $defaultvalues,
      '#empty' => t('No data available.'),
      '#disabled' => ($_SESSION['islandora_batch_modify_finished']),
    );
    if ($ingest_objects) {
      foreach ($ingest_objects as $objectid => $modifywhats) {
        foreach ($modifywhats as $modifywhat => $modifyinfo) {
          $key = $objectid . '_-_' . $modifywhat;
          $object = NULL;
          $is_collection = FALSE;
          if (isset($modifyinfo['object'])) {
            $object = $modifyinfo['object'];
            $is_collection = in_array('islandora:collectionCModel', $object->models);
            $options[$key]['objectid'] = array(
              'data' => array(
                '#type' => 'link',
                '#title' => filter_xss($objectid),
                '#href' => "islandora/object/{$object->id}",
              ),
            );
          }
          else {
            $options[$key]['objectid'] = check_plain($objectid);
          }
          if (isset($modifyinfo['value'])) {
            $options[$key]['value'] = $modifyinfo["value"];
          }
          else {
            $options[$key]['value'] = "-";
          }
          $options[$key]['what'] = $modifywhat;
          $options[$key]['action'] = $modifyinfo['action'];
          $defaultvalues[$key] = $modifyinfo["hasaction"];
        }
      }
    }
    $table['#options'] = $options;
    $table['#default_value'] = $defaultvalues;
    $form['table'] = $table;

    $form['cancel'] = array(
      '#type' => 'submit',
      '#value' => t('Back to upload'),
    );
    if (!$_SESSION['islandora_batch_modify_finished']) {
      $form['submit'] = array(
        '#type' => 'submit',
        '#value' => t('Batch Modify'),
      );
    }
  }

  return $form;
}

/**
 * Submitting the form for confirming the ingest of datastreams.
 */
function islandora_batch_modify_confirm_form_submit(array $form, array &$form_state) {
  if ($form_state['clicked_button']['#id'] === 'edit-cancel') {
    unset($_SESSION['islandora_batch_modify_ingest_objects']);
    unset($_SESSION['islandora_batch_modify_finished']);

    return;
  }
  if ($_SESSION['islandora_batch_modify_finished']) {
    // Do not change anything if already finished.
    return;
  }
  $operations = array();
  foreach ($form_state["values"]["table"] as $key => $isneeded) {
    $parts = explode("_-_", $key);
    $objectid = $parts[0];
    $what = $parts[1];
    if ($isneeded) {
      $params = $_SESSION['islandora_batch_modify_ingest_objects'][$objectid][$what];
      if (isset($params['filepath'])) {
        $params['value'] = $params['filepath'];
      }
      $params['what'] = $what;

      $_SESSION['islandora_batch_modify_ingest_objects'][$objectid][$what]['hasaction'] = TRUE;

      $operations[] = array('islandora_batch_modify_operation', array($params));
    }
    else {
      $_SESSION['islandora_batch_modify_ingest_objects'][$objectid][$what]['hasaction'] = FALSE;
    }
  }
  if (count($operations) > 0) {
    $batch = array(
      'operations' => $operations,
      'title' => t("Ingesting datastreams"),
      'init_message' => t("Preparing ..."),
      'progress_message' => t('Time elapsed: @elapsed <br/>Estimated time remaining @estimate.'),
      'error_message' => t('An error has occurred.'),
      'file' => drupal_get_path('module', 'islandora_batch_modify') . '/includes/zip_import.form.inc',
      'finished' => 'islandora_batch_modify_operation_finished',
    );
    batch_set($batch);
  }
}

/**
 * Process the ingestion of datastreams per object.
 */
function islandora_batch_modify_operation($params, &$context) {
  if (array_key_exists('object', $params)) {
    module_load_include('inc', 'islandora_batch_modify', 'includes/utilities');

    $object = $params['object'];
    $modifywhat = $params['what'];

    islandora_batch_modify_do_work_for_one($object, $modifywhat, $params, 'drupal_set_message', 'error');
  }
}

/**
 * Finish process the ingestion of datastreams per object.
 */
function islandora_batch_modify_operation_finished() {
  $_SESSION['islandora_batch_modify_finished'] = TRUE;
}

