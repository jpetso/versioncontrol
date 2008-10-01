<?php
// $Id$
/**
 * @file
 * Version Control API - An interface to version control systems
 * whose functionality is provided by pluggable back-end modules.
 *
 * This file provides support for workflow-ng (will be Rules in Drupal 6).
 *
 * Copyright 2007, 2008 by Jakob Petsovits ("jpetso", http://drupal.org/user/56020)
 */

// First up, token integration. This is not in the main module because no
// module other than workflow_ng can currently make use of it.

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


// Secondly, workflow_ng integration.

/**
 * Implementation of hook_entity_into().
 */
function versioncontrol_entity_info() {
  return array(
    'versioncontrol_repository' => array(),
    'versioncontrol_operation' => array(),
    'versioncontrol_item_list' => array(),
  );
}

/**
 * Implementation of hook_event_info().
 */
function versioncontrol_event_info() {
  return array(
    'versioncontrol_operation_insert' => array(
      '#label' => t('Version control operation has been recorded'),
      '#description' => t('A "version control operation" is either a commit or the creation/deletion of a branch or tag. Note that this event is normally called from either cron or an external script, so you shouldn\'t do things like page redirections, message popups or similar on-site actions.'),
      '#module' => t('Version Control API'),
      '#arguments' => array(
        'operation' => array(
          '#entity' => 'versioncontrol_operation',
          '#label' => t('Operation'),
        ),
        'author' => array(
          '#entity' => 'user',
          '#label' => t('Author'),
          '#handler' => 'versioncontrol_events_argument_get_author',
        ),
        'repository' => array(
          '#entity' => 'versioncontrol_repository',
          '#label' => t('Repository'),
          '#handler' => 'versioncontrol_events_argument_get_repository',
        ),
        'items' => array(
          '#entity' => 'versioncontrol_item_list',
          '#label' => t('Items (files/directories)'),
        ),
      ),
    ),
  );
}

function versioncontrol_events_argument_get_author($operation) {
  return user_load(array('uid' => $operation['uid']));
}

function versioncontrol_events_argument_get_repository($operation) {
  return $operation['repository'];
}

/**
 * Implementation of hook_condition_info().
 */
function versioncontrol_condition_info() {
  return array(
    'versioncontrol_condition_repository' => array(
      '#label' => t('Compare repository'),
      '#arguments' => array(
        'repository' => array(
          '#entity' => 'versioncontrol_repository',
          '#label' => t('Repository to compare'),
        ),
      ),
      '#description' => t('Evaluates to TRUE if a repository is one of several selected repositories.'),
      '#module' => t('Version Control API'),
    ),
    'versioncontrol_condition_operation_labels' => array(
      '#label' => t('Compare branch/tag name'),
      '#arguments' => array(
        'items' => array(
          '#entity' => 'versioncontrol_operation',
          '#label' => t('Operation (commit, or creation/deletion of a branch or tag)'),
        ),
      ),
      '#description' => t('Evaluates to TRUE if the version control operation affected a branch or tag of the given name.'),
      '#module' => t('Version Control API'),
    ),
    'versioncontrol_condition_item_paths' => array(
      '#label' => t('Compare item paths'),
      '#arguments' => array(
        'items' => array(
          '#entity' => 'versioncontrol_item_list',
          '#label' => t('Set of items to be compared'),
        ),
      ),
      '#description' => t('Evaluates to TRUE if the path of at least one of the items matches a given set of regular expressions.'),
      '#module' => t('Version Control API'),
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

function versioncontrol_condition_repository_form($settings = array(), $argument_info) {
  $repository_options = array();
  foreach (versioncontrol_get_repositories() as $repo_id => $repository) {
    $repository_options[$repo_id] = check_plain($repository['name']);
  }

  $form['repo_ids'] = array(
    '#type' => 'checkboxes',
    '#title' => t('Matched repositories'),
    '#description' => t('If the repository is in the selected set of repositories, this condition will return TRUE.'),
    '#options' => $repository_options,
    '#default_value' => empty($settings['repo_ids'])
                        ? array()
                        : $settings['repo_ids'],
    '#required' => TRUE,
  );
  return $form;
}

function versioncontrol_condition_repository_submit($form_id, $form_values) {
  return array(
    'repo_ids' => array_filter(array_keys(array_filter($form_values['repo_ids']))),
  );
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

function versioncontrol_condition_item_paths_form($settings = array(), $argument_info) {
  $form['item_paths']['path_regexps'] = array(
    '#type' => 'textfield',
    '#title' => t('List of regular expressions'),
    '#description' => t('If the paths of the items (relative to the repository root, starting with "/") match one of these PHP regular expressions, this condition will return TRUE. Separate the regular expressions from each other with spaces. Example: "@^/trunk/@ @^/contributions/profiles.*(?&lt;!\.profile|\.txt)$@ @^.*\.(gz|tgz|tar|zip)$@"'),
    '#default_value' => empty($settings['path_regexps'])
                        ? '@^/@'
                        : implode(' ', $settings['path_regexps']),
    '#required' => TRUE,
    '#size' => 60,
  );
  $form['item_paths']['operation'] = array(
    '#type' => 'select',
    '#title' => t('How many items must match one of the regular expressions'),
    '#options' => array('OR' => t('It\'s enough if any item matches'), 'AND' => t('All items must match')),
    '#description' => t('Specify whether any item or all items must match one of the regular expressions for the condition to return TRUE.'),
    '#default_value' => $settings['operation'],
  );
  return $form;
}

function versioncontrol_condition_item_paths_submit($form_id, $form_values) {
  return array(
    'path_regexps' => array_filter(explode(' ', $form_values['path_regexps'])),
    'operation' => $form_values['operation'],
  );
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

function versioncontrol_condition_operation_labels_form($settings = array(), $argument_info) {
  $form['labels']['label_regexps'] = array(
    '#type' => 'textfield',
    '#title' => t('List of regular expressions'),
    '#description' => t('If a branch/tag name matches one of these PHP regular expressions, this condition will return TRUE. Separate the regular expressions from each other with spaces. Example (for a branch name, in this case): "@^HEAD$@ @^DRUPAL-5(--[2-9])?$@ @^DRUPAL-6--[1-9]$@"'),
    '#default_value' => empty($settings['label_regexps'])
                        ? ''
                        : implode(' ', $settings['label_regexps']),
    '#required' => TRUE,
    '#size' => 60,
  );
  return $form;
}

function versioncontrol_condition_operation_labels_submit($form_id, $form_values) {
  return array(
    'label_regexps' => array_filter(explode(' ', $form_values['label_regexps'])),
  );
}
