<?php
/**
 * @file
 * Functions for batch modifications via drush.
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
 * Implements hook_drush_command().
 */
function islandora_batch_modify_drush_command() {
  $items['islandora_batch_modify'] = array(
    'description' => 'Ingests or generates datastreams for objects found in a CSV file.',
    'options' => array(
      'csvfile' => 'An absolute path to an existing CSV file, containing lines with 3 values: an object id, a datastream id or property type, and optionally a absolute path to a datastream file or property value.',
    ),
    'aliases' => array('ibm'),
    'examples' => array(
      'drush --user=admin islandora_batch_modify --csvfile=/tmp/test.csv',
      'drush --user=admin ibm --csvfile=/tmp/test.csv',
    ),
  );
  return $items;
}

/**
 * Implements drush_hook_COMMAND_validate().
 */
function drush_islandora_batch_modify_validate() {
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
function drush_islandora_batch_modify() {
  module_load_include('inc', 'islandora_batch_modify', 'includes/utilities');
  $csvfile = drush_get_option('csvfile');

  $workinfo = islandora_batch_modify_parse_csv_file($csvfile, 'drush_log'); 

  if ($workinfo) {
    islandora_batch_modify_do_work($workinfo, 'drush_log', 'notice');
  }
  else {
    drush_log(t('Nothing to do!'), 'notice');
  }
}
