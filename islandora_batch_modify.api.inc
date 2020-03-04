<?php

/**
 * @file
 * islandora_batch_modify.api
 *
 *
 *  Copyright 2020 Leiden University Library
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
 * Hook to generate extra derivatives.
 *
 * @return array
 *   An array of messages describing the outcome of the derivative events.
 *   Each individual message array has the following structure:
 *   - success: Bool denoting whether the operation was successful.
 *   - messages: An array structure containing:
 *     - message: A string passed through t() describing the
 *       outcome of the operation.
 *     - message_sub: (Optional) Substitutions to be passed along to t() or
 *       watchdog.
 *     - type: A string denoting whether the output is to be
 *       drupal_set_messaged (dsm) or watchdogged (watchdog).
 *     - severity: (Optional) A severity level / status to be used when
 *       logging messages. Uses the defaults of drupal_set_message and
 *       watchdog if not defined.
 */
function hook_cmodel_pid_generate_extra_derivatives($object, $what, $options = array(), $logfunction = NULL, $loglevel = NULL) {
  // Generate a derivative indicated by $what for $object.
}
