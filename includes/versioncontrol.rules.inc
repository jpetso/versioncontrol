<?php
// $Id$
/**
 * @file
 * Version Control API - An interface to version control systems
 * whose functionality is provided by pluggable back-end modules.
 *
 * This file provides support for Rules.
 *
 * Copyright 2007, 2008 by Jakob Petsovits ("jpetso", http://drupal.org/user/56020)
 */

// First up, token integration. This is not in the main module because no
// module other than Rules can currently make use of it.

/**
 * Implementation of hook_token_list().
 */
function versioncontrol_token_list($type = 'all') {
  $tokens = array();

  if ($type == 'versioncontrol_repository' || $type == 'all') {
    $repo_name_mapping = array();
    foreach (versioncontrol_get_repositories() as $repo_id => $repository) {
      $repo_name_mapping[] = t('!id for @repo-name', array(
        '!id' => $repo_id,
        '@repo-name' => $repository['name'],
      ));
    }

    $vcs_mapping = array();
    foreach (versioncontrol_get_backends() as $vcs => $backend) {
      $vcs_mapping[$vcs] = check_plain($backend['name']);
    }

    $tokens['versioncontrol repository'] = array(
      'id' => t('Repository identifier (known ids: !repo-mapping)', array(
        '!repo-mapping' => implode(', ', $repo_name_mapping),
      )),
      'name' => t('Repository name, as listed in the above mapping'),
      'vcs' => t('VCS backend identifier (known backend ids: !backends)', array(
        '!backends' => implode(', ', array_keys($vcs_mapping)),
      )),
      'vcs-name' => t('VCS backend name (known backend names: !backends)', array(
        '!backends' => implode(', ', $vcs_mapping),
      )),
      'root' => t('Repository root'),
    );
  }
  if ($type == 'versioncontrol_operation' || $type == 'all') {
    $tokens['versioncontrol operation'] = array(
      'id' => t('Operation identifier (vc_op_id)'),
      'date' => t('Commit date'),
      'revision' => t('Global revision identifier'),
      'vcs-user' => t('VCS specific account name'),
      'vcs-user-raw' => t('VCS specific account name. WARNING - raw unfiltered message, not safe for HTML (fine for emails, though).'),
      'message' => t('Commit/branch/tag message (might be empty for branches and tags)'),
      'message-raw' => t('Commit/branch/tag message like above. WARNING - raw unfiltered message, not safe for HTML (fine for emails, though).'),
    );
  }
  return $tokens;
}

/**
 * Implementation of hook_token_values().
 */
function versioncontrol_token_values($type, $object = NULL) {
  switch ($type) {
    case 'versioncontrol_repository':
      $repository = $object;
      $backend = versioncontrol_get_backend($repository['vcs']);
      $values = array(
        'id' => $repository['repo_id'],
        'name' => check_plain($repository['name']),
        'vcs' => check_plain($repository['vcs']),
        'vcs-name' => check_plain($backend['name']),
        'root' => check_plain($backend['root']),
      );
      break;
    case 'versioncontrol_operation':
      $operation = $object;
      $values = array(
        'id' => $operation['vc_op_id'],
        'revision' => $operation['revision'],
        'vcs-user' => check_plain($operation['username']),
        'vcs-user-raw' => $operation['username'],
        'message' => check_plain($operation['message']),
        'message-raw' => $operation['message'],
        'date' => format_date($operation['date'],
          'short', '', variable_get('date_default_timezone', 0)
        ),
      );
      break;
  }
  return $values;
}


// Secondly, Rules integration.

/**
 * Implementation of hook_rules_data_type_info().
 */
function versioncontrol_rules_data_type_info() {
  return array(
    'versioncontrol_repository' => array(
      'label' => t('Repository'),
      'class' => 'versioncontrol_data_type_repository',
      // The versioncontrol_data_type_repository class also supports
      // 'savable' == TRUE, but there's no use case for it currently.
      'savable' => FALSE,
      'identifiable' => TRUE,
      'module' => t('Version Control API'),
    ),
    'versioncontrol_operation' => array(
      'label' => t('Version control operation'),
      'class' => 'versioncontrol_data_type_operation',
      'savable' => FALSE,
      'identifiable' => TRUE,
      'module' => t('Version Control API'),
    ),
    'versioncontrol_item_list' => array(
      'label' => t('List of repository items (files/directories)'),
      'class' => 'rules_data_type',
      'savable' => FALSE,
      'identifiable' => FALSE,
      'module' => t('Version Control API'),
    ),
  );
}

/**
 * Defines the versioncontrol_repository data type for Rules.
 */
class versioncontrol_data_type_repository extends rules_data_type {
  function save() {
    $repository = &$this->get();
    $repository_urls = _versioncontrol_get_repository_urls($repository);
    versioncontrol_update_repository($repository);
    return TRUE;
  }

  function load($repo_id) {
    return versioncontrol_get_repository($repo_id);
  }

  function get_identifier() {
    $repository = &$this->get();
    return $repository['repo_id'];
  }
}

/**
 * Defines the versioncontrol_operation data type for Rules.
 */
class versioncontrol_data_type_operation extends rules_data_type {
  function load($vc_op_id) {
    $operations = versioncontrol_get_operations(array('vc_op_ids' => array($vc_op_id)));
    return empty($operations) ? FALSE : reset($operation); // first (only) element
  }

  function get_identifier() {
    $operation = &$this->get();
    return $operation['vc_op_id'];
  }
}


/**
 * Implementation of hook_rules_event_info().
 */
function versioncontrol_rules_event_info() {
  return array(
    'versioncontrol_operation_insert' => array(
      'label' => t('Version control operation has been recorded'),
      'help' => t('A "version control operation" is either a commit or the creation/deletion of a branch or tag. Note that this event is normally called from either cron or an external script, so you shouldn\'t do things like page redirections, message popups or similar on-site actions.'),
      'module' => t('Version Control API'),
      'arguments' => array(
        'operation' => array(
          'type' => 'versioncontrol_operation',
          'label' => t('Operation'),
        ),
        'author' => array(
          'type' => 'user',
          'label' => t('Author'),
          'handler' => 'versioncontrol_events_argument_get_author',
        ),
        'repository' => array(
          'type' => 'versioncontrol_repository',
          'label' => t('Repository'),
          'handler' => 'versioncontrol_events_argument_get_repository',
        ),
        'message' => array(
          'type' => 'string',
          'label' => t('Log message'),
          'handler' => 'versioncontrol_events_argument_get_message',
        ),
        'date' => array(
          'type' => 'date',
          'label' => t('Date'),
          'handler' => 'versioncontrol_events_argument_get_date',
        ),
        'items' => array(
          'type' => 'versioncontrol_item_list',
          'label' => t('Items (files/directories)'),
        ),
      ),
    ),
  );
}

function versioncontrol_events_argument_get_author($operation) {
  return user_load($operation['uid']);
}

function versioncontrol_events_argument_get_repository($operation) {
  return $operation['repository'];
}

function versioncontrol_events_argument_get_message($operation) {
  return $operation['message'];
}

function versioncontrol_events_argument_get_date($operation) {
  // Looking at the source code of rules.rules.inc, this seems to be the
  // native date representation of the rules_data_type_date class.
  return gmdate('Y-m-d H:i:s', $operation['date']);
}

/**
 * Implementation of hook_rules_condition_info().
 */
function versioncontrol_rules_condition_info() {
  return array(
    'versioncontrol_condition_repository' => array(
      'label' => t('Compare repository'),
      'arguments' => array(
        'repository' => array(
          'entity' => 'versioncontrol_repository',
          'label' => t('Repository to compare'),
        ),
      ),
      'help' => t('Evaluates to TRUE if a repository is one of several selected repositories.'),
      'module' => t('Version Control API'),
    ),
    'versioncontrol_condition_operation_labels' => array(
      'label' => t('Compare branch/tag name'),
      'arguments' => array(
        'items' => array(
          'entity' => 'versioncontrol_operation',
          'label' => t('Operation (commit, or creation/deletion of a branch or tag)'),
        ),
      ),
      'help' => t('Evaluates to TRUE if the version control operation affected a branch or tag of the given name.'),
      'module' => t('Version Control API'),
    ),
    'versioncontrol_condition_item_paths' => array(
      'label' => t('Compare item paths'),
      'arguments' => array(
        'items' => array(
          'entity' => 'versioncontrol_item_list',
          'label' => t('Set of items to be compared'),
        ),
      ),
      'help' => t('Evaluates to TRUE if the path of at least one of the items matches a given set of regular expressions.'),
      'module' => t('Version Control API'),
    ),
  );
}


// Condition implementations.

/**
 * "Compare repository" condition:
 * Evaluates to TRUE if a repository is one of several selected repositories.
 */
function versioncontrol_condition_repository($repository, $settings) {
  return in_array($repository['repo_id'], $settings['repo_ids']);
}

function versioncontrol_condition_repository_form($settings, &$form) {
  $repository_options = array();
  foreach (versioncontrol_get_repositories() as $repo_id => $repository) {
    $repository_options[$repo_id] = check_plain($repository['name']);
  }

  $form['settings']['repo_ids'] = array(
    '#type' => 'checkboxes',
    '#title' => t('Matched repositories'),
    '#description' => t('If the repository is in the selected set of repositories, this condition will return TRUE.'),
    '#options' => $repository_options,
    '#default_value' => empty($settings['repo_ids'])
                        ? array()
                        : $settings['repo_ids'],
    '#required' => TRUE,
  );
}

function versioncontrol_condition_repository_submit(&$settings, $form, $form_state) {
  $settings['repo_ids'] = array_filter(array_keys(array_filter($settings['repo_ids'])));
}


/**
 * "Compare item location" condition:
 * Evaluates to TRUE if the path of at least one of the items matches a given
 * set of regular expressions.
 */
function versioncontrol_condition_item_paths($items, $settings) {
  $all_items_required = ($settings['operation'] == 'AND');

  foreach ($items as $path => $item) {
    if ($all_items_required) {
      $item_matches = FALSE;
    }
    // Here comes the actual comparison.
    foreach ($settings['path_regexps'] as $path_regexp) {
      if (versioncontrol_preg_item_match($path_regexp, $item)) {
        if ($all_items_required) { // 'AND' mode: this item is ok, next one
          $item_matches = TRUE;
          break;
        }
        else { // 'OR' mode: a single matching item is enough, return TRUE
          return TRUE;
        }
      }
    }
    // This item didn't match any of the regexps
    // -> not all items match one of the regexps
    // -> condition has not been met.
    if ($all_items_required && !$item_matches) {
      return FALSE;
    }
  }

  // If we made it here in 'AND' mode, all items match.
  if ($all_items_required) {
    return TRUE;
  }
  // In 'OR' mode, any matching item would have returned TRUE,
  // which means no single item has matched if we made it here.
  return FALSE;
}

function versioncontrol_condition_item_paths_form($settings, &$form) {
  $form['settings']['item_paths']['path_regexps'] = array(
    '#type' => 'textfield',
    '#title' => t('List of regular expressions'),
    '#description' => t('If the paths of the items (relative to the repository root, starting with "/") match one of these PHP regular expressions, this condition will return TRUE. Separate the regular expressions from each other with spaces. Example: "@^/trunk/@ @^/contributions/profiles.*(?&lt;!\.profile|\.txt)$@ @^.*\.(gz|tgz|tar|zip)$@"'),
    '#default_value' => empty($settings['path_regexps'])
                        ? '@^/@'
                        : implode(' ', $settings['path_regexps']),
    '#required' => TRUE,
    '#size' => 60,
  );
  $form['settings']['item_paths']['operation'] = array(
    '#type' => 'select',
    '#title' => t('How many items must match one of the regular expressions'),
    '#options' => array('OR' => t('It\'s enough if any item matches'), 'AND' => t('All items must match')),
    '#description' => t('Specify whether any item or all items must match one of the regular expressions for the condition to return TRUE.'),
    '#default_value' => $settings['operation'],
  );
}

function versioncontrol_condition_item_paths_submit(&$settings, $form, $form_state) {
  $settings['path_regexps'] = array_filter(explode(' ', $settings['path_regexps']));
}


/**
 * "Compare item location" condition:
 * Evaluates to TRUE if the path of at least one of the items matches a given
 * set of regular expressions.
 */
function versioncontrol_condition_operation_labels($operation, $settings) {
  // Might not cover all use cases if there are multiple labels in one
  // operation, especially with "deleted" label actions.
  // Probably need to distinguish between operation types and label actions.
  foreach ($operation['labels'] as $label) {
    foreach ($settings['label_regexps'] as $label_regexp) {
      if (preg_match($path_regexp, $label['name'])) {
        return TRUE;
      }
    }
  }
  return FALSE;
}

function versioncontrol_condition_operation_labels_form($settings, &$form) {
  $form['settings']['labels']['label_regexps'] = array(
    '#type' => 'textfield',
    '#title' => t('List of regular expressions'),
    '#description' => t('If a branch/tag name matches one of these PHP regular expressions, this condition will return TRUE. Separate the regular expressions from each other with spaces. Example (for a branch name, in this case): "@^HEAD$@ @^DRUPAL-5(--[2-9])?$@ @^DRUPAL-6--[1-9]$@"'),
    '#default_value' => empty($settings['label_regexps'])
                        ? ''
                        : implode(' ', $settings['label_regexps']),
    '#required' => TRUE,
    '#size' => 60,
  );
}

function versioncontrol_condition_operation_labels_submit(&$settings, $form, $form_state) {
  $settings['label_regexps'] = array_filter(explode(' ', $settings['label_regexps']));
}
