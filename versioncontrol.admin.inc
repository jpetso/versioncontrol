<?php
// $Id$
/**
 * @file
 * Version Control API - An interface to version control systems
 * whose functionality is provided by pluggable back-end modules.
 *
 * This file contains the administrative user interface
 * for accounts and repositories.
 *
 * Copyright 2006, 2007 Derek Wright ("dww" , http://drupal.org/user/46549)
 * Copyright 2007, 2008, 2009 by Jakob Petsovits ("jpetso", http://drupal.org/user/56020)
 */

/**
 * Form callback for "admin/project/versioncontrol-settings/general":
 * Provide a form for settings of Version Control API itself.
 */
function versioncontrol_admin_settings(&$form_state) {
  $form = array();
  $presets = _versioncontrol_get_string_presets();

  $form['#id'] = 'versioncontrol-settings-form';

  $form['versioncontrol_email_address'] = array(
    '#type' => 'textfield',
    '#title' => t('E-mail address'),
    '#default_value' => variable_get('versioncontrol_email_address', 'versioncontrol@example.com'),
    '#description' => t('The e-mail address of the VCS administrator.'),
    '#weight' => -10,
  );

  $form['versioncontrol_allow_unauthorized_access'] = array(
    '#type' => 'checkbox',
    '#title' => 'Allow repository access to unknown or unauthorized committers',
    '#description' => 'If this is enabled, VCS users that are not registered or approved can nevertheless commit to repositories, assuming that no other commit restrictions apply. If it is disabled, only authorized users may commit. In any case, this setting only works if the appropriate pre-commit/pre-branch/pre-tag hook script is in place for the respective repository.',
    '#default_value' => variable_get('versioncontrol_allow_unauthorized_access', 0),
    '#weight' => -1,
  );

  $form['versioncontrol_register_form'] = array(
    '#type' => 'fieldset',
    '#title' => t('Account registration form strings'),
    '#description' => t('The following messages are shown on the !repository-selection-page that leads to the account registration form.', array('!repository-selection-page' => l(t('repository selection page'), 'versioncontrol/register/demo'))),
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
    '#weight' => 0,
  );
  $form['versioncontrol_register_form']['versioncontrol_registration_message_unauthorized'] = array(
    '#type' => 'textarea',
    '#title' => t('Message to unauthorized users'),
    '#description' => t('Message to show to anonymous users and to users without the \'use version control systems\' permission.'),
    '#default_value' => variable_get(
      'versioncontrol_registration_message_unauthorized',
      $presets['versioncontrol_registration_message_unauthorized']
    ),
  );
  $form['versioncontrol_register_form']['versioncontrol_registration_message_authorized'] = array(
    '#type' => 'textarea',
    '#title' => t('Message to registering users'),
    '#description' => t('Message to show to users who are in the process of selecting a repository in order to create a VCS account. If there\'s only one repository on this site, users will never get to see this message.'),
    '#default_value' => variable_get(
      'versioncontrol_registration_message_authorized',
      $presets['versioncontrol_registration_message_authorized']
    ),
  );

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save configuration'),
    '#weight' => 100,
  );

  return $form;
}

/**
 * Submit handler for the authorization settings form.
 * system_settings_form() is left out because it's nasty for modules that
 * hook into this form and don't want to use automatic variable saving.
 */
function versioncontrol_admin_settings_submit($form, &$form_state) {
  $value_names = array(
    'versioncontrol_email_address',
    'versioncontrol_allow_unauthorized_access',
    'versioncontrol_registration_message_unauthorized',
    'versioncontrol_registration_message_authorized',
  );
  foreach ($value_names as $name) {
    variable_set($name, $form_state['values'][$name]);
  }
}


/**
 * Form callback for "admin/project/versioncontrol-accounts":
 * A list of accounts with filtering possibilities.
 */
function versioncontrol_admin_account_list_page(&$form_state) {
  $users = array();
  $accounts = versioncontrol_get_accounts(array(), TRUE);

  // Retrieve all repositories that users are listed in.
  $repo_ids = array();
  foreach ($accounts as $uid => $usernames_by_repository) {
    foreach ($usernames_by_repository as $repo_id => $username) {
      $repo_ids[] = $repo_id;
    }
  }
  $repo_ids = array_unique($repo_ids);
  $repositories = versioncontrol_get_repositories(array('repo_ids' => $repo_ids));
  $filter_form = drupal_get_form('versioncontrol_admin_account_list_filter_form', $repositories);

  // Filter the list of accounts.
  foreach (module_implements('versioncontrol_filter_accounts') as $module) {
    $function = $module .'_versioncontrol_filter_accounts';
    $function($accounts);
  }
  $accounts = versioncontrol_admin_get_paged_accounts($accounts);

  // Retrieve more info for the accounts that survived filtering.
  foreach ($accounts as $uid => $usernames_by_repository) {
    $user = user_load(array('uid' => $uid));
    if (!$user) {
      unset($accounts[$uid]);
      continue;
    }
    $users[$uid] = $user;
  }
  if (empty($users)) {
    return $filter_form .'<p>'.
      t('No accounts could be found with the current filter settings.')
      .'</p>';
  }

  $placeholders = array();
  $params = array();
  foreach ($users as $uid => $user) {
    $placeholders[] = '%d';
    $params[] = $uid;
  }

  // Retrieve total, first and last commits of each user.
  $params[] = VERSIONCONTROL_OPERATION_COMMIT;
  $result = db_query('SELECT uid, COUNT(vc_op_id) AS number_commits,
                       MIN(date) AS first_commit_date, MAX(date) AS last_commit_date,
                       MIN(vc_op_id) AS first_commit_id, MAX(vc_op_id) AS last_commit_id
                      FROM {versioncontrol_operations}
                      WHERE type = %d
                       AND uid IN ('. implode(',', $placeholders) .')
                      GROUP BY uid', $params);

  $number_commits = array();
  $first_commits = array();
  $last_commits = array();

  while ($statistics = db_fetch_object($result)) {
    $number_commits[$statistics->uid] = $statistics->number_commits;
    $first_commits[$statistics->uid] = t('!time ago', array(
      '!time' => format_interval(time() - $statistics->first_commit_date, 1))
    );
    $last_commits[$statistics->uid] = t('!time ago', array(
      '!time' => format_interval(time() - $statistics->last_commit_date, 1))
    );
    if (module_exists('commitlog')) {
      $number_commits[$statistics->uid] =
        l($number_commits[$statistics->uid], 'user/'. $statistics->uid .'/track/code');
      $first_commits[$statistics->uid] =
        theme('commitlog_vc_op_id', $statistics->first_commit_id, $first_commits[$statistics->uid]);
      $last_commits[$statistics->uid] =
        theme('commitlog_vc_op_id', $statistics->last_commit_id, $last_commits[$statistics->uid]);
    }
  }

  // Construct the user account table.
  $header = array(t('User'), t('Accounts'), t('Commits'), t('First commit'), t('Last commit'));
  $rows_by_uid = array();
  foreach ($accounts as $uid => $usernames_by_repository) {
    $user = $users[$uid];

    // Present a list of all VCS usernames and the repository where they're in.
    $repo_usernames = array();
    foreach ($usernames_by_repository as $repo_id => $username) {
      if (!isset($repositories[$repo_id])) { // happens if the backend isn't loaded
        continue;
      }
      $username = theme('versioncontrol_account_username',
        $uid, $username, $repositories[$repo_id], FALSE
      );
      if (module_exists('commitlog')) {
        $repo_name = theme('commitlog_repository', $repositories[$repo_id]);
      }
      else {
        $repo_name = check_plain($repositories[$repo_id]['name']);
      }

      $repo_usernames[] = t('!username in !repository (!edit)', array(
        '!username' => $username,
        '!repository' => $repo_name,
        '!edit' => l(t('edit'), 'user/'. $uid .'/edit/versioncontrol/'. $repo_id),
      ));
    }
    $vcs_usernames = empty($repo_usernames)
      ? t('VCS backend is currently disabled')
      : theme('item_list', $repo_usernames);

    $rows_by_uid[$uid] = array(
      theme('username', $user), $vcs_usernames,
      isset($number_commits[$uid]) ? $number_commits[$uid] : 0,
      isset($first_commits[$uid]) ? $first_commits[$uid] : t('n/a'),
      isset($last_commits[$uid]) ? $last_commits[$uid] : t('n/a'),
    );
  }

  // Provide a possibility for backends and other modules to modify the list.
  foreach (module_implements('versioncontrol_alter_account_list') as $module) {
    $function = $module .'_versioncontrol_alter_account_list';
    $function($accounts, $repositories, $header, $rows_by_uid);
  }
  // We don't want the uids in the final $rows array.
  $rows = array_values($rows_by_uid);

  // The finished user account list with account filter.
  $output .= $filter_form . theme('table', $header, $rows);

  if ($pager = theme('pager', NULL, variable_get('versioncontrol_admin_account_pager', 20), 0)) {
    $output .= $pager;
  }

  return $output;
}

/**
 * Return a subset of the given accounts (10 Drupal users by default, and all
 * of their VCS usernames). Paging is also used by emulating pager_query().
 */
function versioncontrol_admin_get_paged_accounts($accounts, $element = 0) {
  global $pager_page_array, $pager_total, $pager_total_items;
  $page = isset($_GET['page']) ? $_GET['page'] : '';
  $pager_page_array = explode(',', $page);
  $page = empty($pager_page_array[$element]) ? 0 : $pager_page_array[$element];

  $limit = variable_get('versioncontrol_admin_account_pager', 20);
  $i = 0;
  $paged_accounts = array();

  foreach ($accounts as $uid => $usernames_by_repository) {
    if ($i >= ($page * $limit) && $i < (($page+1) * $limit)) {
      $paged_accounts[$uid] = $usernames_by_repository;
    }
    ++$i;
  }

  // Emulate pager_query() in order to get a proper theme('pager').
  $pager_total_items[$element] = count($accounts);
  $pager_total[$element] = ceil($pager_total_items[$element] / $limit);
  $pager_page_array[$element] = max(0, min((int)$pager_page_array[$element], ((int)$pager_total[$element]) - 1));

  return $paged_accounts;
}

/**
 * A form for filtering accounts which is embedded in the account list page.
 */
function versioncontrol_admin_account_list_filter_form(&$form_state, $repositories) {
  $form = array();
  $repository_names = array(0 => t('All'));

  foreach ($repositories as $repo_id => $repository) {
    $repository_names[$repo_id] = check_plain($repository['name']);
  }
  if (!isset($_SESSION['versioncontrol_filter_repo_id'])) {
    $_SESSION['versioncontrol_filter_repo_id'] = 0;
  }

  $form['#id'] = 'versioncontrol-account-filter-form';

  $form['fieldset'] = array(
    '#type' => 'fieldset',
    '#title' => t('Filter'),
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
  );
  $form['fieldset']['filters'] = array(
    '#value' => '',
    '#prefix' => '<div class="container-inline">',
    '#suffix' => '</div>',
  );
  $form['fieldset']['filters']['repo_id'] = array(
    '#type' => 'select',
    '#title' => t('Repository'),
    '#options' => $repository_names,
    '#default_value' => $_SESSION['versioncontrol_filter_repo_id'],
    '#weight' => 10,
  );
  $form['fieldset']['filters']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Filter'),
    '#weight' => 100,
  );
  return $form;
}

/**
 * Submit handler for the account filter form.
 */
function versioncontrol_admin_account_list_filter_form_submit($form, &$form_state) {
  $_SESSION['versioncontrol_filter_repo_id'] = $form_state['values']['repo_id'];
}

/**
 * Implementation of hook_versioncontrol_filter_accounts():
 * Unset filtered accounts before they are even attempted to be displayed
 * on the account list.
 *
 * @param $accounts
 *   The accounts that would normally be displayed, in the same format as the
 *   return value of versioncontrol_get_accounts(). Entries in this list
 *   may be unset by this filter function.
 */
function versioncontrol_versioncontrol_filter_accounts(&$accounts) {
  if (empty($accounts)) {
    return;
  }
  if (!isset($_SESSION['versioncontrol_filter_repo_id'])) {
    $_SESSION['versioncontrol_filter_repo_id'] = 0;
  }
  $filter_repo_id = $_SESSION['versioncontrol_filter_repo_id'];

  if ($filter_repo_id == 0) {
    return; // Don't change the list if "All" repositories are selected.
  }

  foreach ($accounts as $uid => $usernames_by_repository) {
    foreach ($usernames_by_repository as $repo_id => $username) {
      if ($repo_id != $filter_repo_id) {
        unset($accounts[$uid][$repo_id]);
        if (empty($accounts[$uid])) {
          unset($accounts[$uid]);
        }
      }
    }
  }
}


/**
 * Form callback for "admin/project/versioncontrol-accounts/import":
 * A repository select box and a text area for VCS specific account data
 * that will be imported into the selected repository.
 */
function versioncontrol_admin_account_import_form(&$form_state) {
  $form = array();
  $selected_backends = array();

  foreach (versioncontrol_get_backends() as $vcs => $backend) {
    if (versioncontrol_backend_implements($vcs, 'import_accounts')) {
      $selected_backends[] = $vcs;
    }
  }
  $repositories = versioncontrol_get_repositories(array('vcs' => $selected_backends));

  if (empty($repositories)) {
    $form['no_repositories'] = array(
      '#type' => 'markup',
      '#value' => t('There are no repositories for which the accounts can be imported.'),
      '#prefix' => '<p>',
      '#suffix' => '</p>',
    );
    return $form;
  }

  $repository_names = array();
  foreach ($repositories as $repo_id => $repository) {
    if (!isset($first_repo_id)) {
      $first_repo_id = $repo_id;
    }
    $repository_names[$repo_id] = check_plain($repository['name']);
  }

  $form['repo_id'] = array(
    '#type' => 'select',
    '#title' => t('Import accounts into'),
    '#options' => $repository_names,
    '#default_value' => $first_repo_id,
  );
  $form['data'] = array(
    '#type' => 'textarea',
    '#title' => t('Account data'),
    '#description' => t('The contents of the config file that contains the accounts that shall be imported. Existing accounts will be updated, new accounts will be created, and all others will be left untouched.'),
    '#cols' => 70,
    '#rows' => 24,
    '#required' => TRUE,
  );
  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Import accounts'),
    '#weight' => 100,
  );
  return $form;
}

/**
 * Validation form callback for vcs account data.
 *
 * Only actually validates that the repo backend is correctly configured.
 */
function versioncontrol_admin_account_import_form_validate($form, &$form_state) {
  if (is_null(versioncontrol_get_repository($form_state['values']['repo_id']))) {
    form_error($form['data'], t('The selected repository could not be found.'));
  }
}

/**
 * Submit handler for the "select repository to import" form from above:
 * Call the [vcs_backend]_admin_import_accounts() with the given data.
 */
function versioncontrol_admin_account_import_form_submit($form, &$form_state) {
  $repository = versioncontrol_get_repository($form_state['values']['repo_id']);
  _versioncontrol_call_backend($repository['vcs'], 'import_accounts', array(
    $repository, $form_state['values']['data']
  ));
}


/**
 * Callback for 'admin/project/versioncontrol-accounts/export/%versioncontrol_repository':
 * Export a repository's authenticated accounts to the version control system's
 * password file format.
 */
function versioncontrol_admin_account_export_page($repository) {
  if (!versioncontrol_backend_implements($repository['vcs'], 'export_accounts')) {
    drupal_not_found();
    return;
  }

  // The repository has already been selected, export the accounts.
  $data = versioncontrol_export_accounts($repository);

  if (arg(5) == 'plaintext') {
    drupal_set_header('Content-Type: text/plain; charset=utf-8');
    print $data;
  }
  else {
    return drupal_get_form(
      'versioncontrol_admin_account_export_display_form', $repository, $data
    );
  }
}

/**
 * Form callback for "admin/project/versioncontrol-accounts/export":
 * Select the repository from which to extract data, and show the exported
 * data on submitting the form.
 */
function versioncontrol_admin_account_export_form(&$form_state) {
  $form = array();
  $selected_backends = array();

  foreach (versioncontrol_get_backends() as $vcs => $backend) {
    if (versioncontrol_backend_implements($vcs, 'export_accounts')) {
      $selected_backends[] = $vcs;
    }
  }
  $repositories = versioncontrol_get_repositories(array('vcs' => $selected_backends));

  if (empty($repositories)) {
    $form['no_repositories'] = array(
      '#type' => 'markup',
      '#value' => t('There are no repositories for which the accounts can be exported.'),
      '#prefix' => '<p>',
      '#suffix' => '</p>',
    );
    return $form;
  }

  $repository_names = array();
  foreach ($repositories as $repo_id => $repository) {
    if (!isset($first_repo_id)) {
      $first_repo_id = $repo_id;
    }
    $repository_names[$repo_id] = check_plain($repository['name']);
  }

  $form['repository'] = array(
    '#type' => 'fieldset',
    '#title' => t('Select repository'),
    '#weight' => 10,
  );
  $form['repository']['repo_id'] = array(
    '#type' => 'select',
    '#title' => t('Export accounts from'),
    '#options' => $repository_names,
    '#default_value' => $first_repo_id,
  );
  $form['repository']['buttons'] = array();
  $form['repository']['buttons']['export_to_form'] = array(
    '#type' => 'submit',
    '#value' => t('Export to page'),
  );
  $form['repository']['buttons']['export_to_plaintext'] = array(
    '#type' => 'submit',
    '#value' => t('Export to plaintext'),
  );
  return $form;
}

/**
 * Submit handler for the "select repository to export" form from above:
 * Redirect to the export form for the specific repository.
 */
function versioncontrol_admin_account_export_form_submit($form, &$form_state) {
  $form_state['redirect'] = 'admin/project/versioncontrol-accounts/export/'. $form_state['values']['repo_id'];

  if ($form_state['values']['op'] == t('Export to plaintext')) {
    $form_state['redirect'] .= '/plaintext';
  }
}

/**
 * Form callback for 'admin/project/versioncontrol-accounts/export/%versioncontrol_repository':
 * Show the exported account data in a form as text area.
 *
 * @param $repository
 *   The repository for which accounts have been exported.
 * @param $data
 *   The exported account data - the text that is supposed to be copied
 *   to the version control system's accounts file.
 */
function versioncontrol_admin_account_export_display_form(&$form_state, $repository, $data) {
  $form = array();

  $backends = versioncontrol_get_backends();
  $vcs_name = $backends[$repository['vcs']]['name'];

  $form['#vcs'] = $vcs_name;
  $form['#repository'] = $repository;

  $form['data'] = array(
    '#type' => 'textarea',
    '#title' => t('@vcs account data', array('@vcs' => $vcs_name)),
    '#description' => t('The exported account data. You can copy this to the config file that contains the @vcs accounts.', array('@vcs' => $vcs_name)),
    '#default_value' => $data,
    '#cols' => 70,
    '#rows' => 30,
  );
  return $form;
}


/**
 * Form callback for "admin/project/versioncontrol-repositories":
 * A simple list of repositories, sorted by version control system.
 */
function versioncontrol_admin_repository_list(&$form_state) {
  $form = array();
  $backends = versioncontrol_get_backends();
  $repositories = versioncontrol_get_repositories();

  if (empty($repositories)) {
    $form['empty'] = array(
      '#value' => '<p>'. t('No repositories are currently known. Please add one or more repositories in order to be able to use version control features on the site.') .'</p>'
    );
    return $form;
  }

  // The header may be modified separately for each VCS,
  // so this is only a template and still missing the Actions column.
  $header_template = array(t('Name'), t('Root'));

  // Sort repositories by backend.
  $repositories_by_backend = array();
  foreach ($repositories as $repo_id => $repository) {
    if (!isset($repositories_by_backend[$repository['vcs']])) {
      $repositories_by_backend[$repository['vcs']] = array();
    }
    $repositories_by_backend[$repository['vcs']][$repo_id] = $repository;
  }

  // Construct the form elements for each VCS separately.
  foreach ($repositories_by_backend as $vcs => $vcs_repositories) {
    $header = $header_template;
    $title = t('@vcs repositories', array('@vcs' => $backends[$vcs]['name']));
    $form[$vcs] = array(
      '#value' => '<h4>'. $title .'</h4>',
    );

    // Add the standard items of each repository - name and root - to the rows.
    $rows_by_repo_id = array();
    foreach ($vcs_repositories as $repo_id => $repository) {
      $rows_by_repo_id[$repo_id] = array(
        check_plain($repository['name']), check_plain($repository['root']),
      );
    }
    // Provide a possibility for backends and other modules to modify the list.
    foreach (module_implements('versioncontrol_alter_repository_list') as $module) {
      $function = $module .'_versioncontrol_alter_repository_list';
      $function($vcs, $vcs_repositories, $header, $rows_by_repo_id);
    }
    // Add the Actions column as final column, after all the other ones
    $header[] = array('data' => t('Actions'), 'colspan' => 2);
    foreach ($rows_by_repo_id as $repo_id => $row) {
      $links = array(
        array(
          'title' => t('Edit'),
          'href' => 'admin/project/versioncontrol-repositories/edit/'. $repo_id,
        ),
        array(
          'title' => t('Delete'),
          'href' => 'admin/project/versioncontrol-repositories/delete/'. $repo_id,
        ),
      );
      $rows_by_repo_id[$repo_id][] = theme('links', $links);
    }
    // We don't want the repository ids in the final $rows array.
    $rows = array_values($rows_by_repo_id);

    // The finished table for the currently processed VCS.
    $form[$vcs]['list'] = array(
      '#value' => theme('table', $header, $rows) .'<br/>'
    );
  }
  return $form;
}

/**
 * Form callback for
 * 'admin/project/versioncontrol-repositories/edit/%versioncontrol_repository'
 * and "admin/project/versioncontrol-repositories/add-$vcs":
 * Provide a form to edit or add a repository.
 *
 * @param $repository
 *   The repository that is to be edited, or VERSIONCONTROL_FORM_CREATE
 *   if the repository doesn't exist yet and should be added.
 * @param $vcs
 *   If $repo_id is NULL, this should be the unique identification string
 *   of the backend for which the repository should be added.
 *   Otherwise, this value is ignored.
 */
function versioncontrol_admin_repository_edit(&$form_state, $repository, $vcs = NULL) {
  $backends = versioncontrol_get_backends();
  $repository_exists = ($repository !== VERSIONCONTROL_FORM_CREATE);

  if (!$repository_exists && !isset($backends[$vcs])) {
    drupal_goto('admin/project/versioncontrol-repositories');
    // drupal_goto() calls exit() so script execution ends right here
  }
  $vcs = $repository_exists ? $repository['vcs'] : $vcs;

  $form = array();
  $form['#id'] = 'versioncontrol-repository-form';
  $form['#vcs'] = $vcs;
  $form['#repository'] = $repository;

  $form['repo_id'] = array(
    '#type' => 'value',
    '#value' => $repository_exists ? $repository['repo_id'] : 0,
  );
  $form['original_name'] = array(
    '#type' => 'value',
    '#value' => $repository_exists ? check_plain($repository['name']) : 0,
  );
  $form['vcs'] = array(
    '#type' => 'value',
    '#value' => $vcs,
  );

  $form['repository_information'] = array(
    '#type' => 'fieldset',
    '#title' => t('Repository information'),
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
    '#weight' => 0,
  );
  $form['repository_information']['repo_name'] = array(
    '#type' => 'textfield',
    '#title' => t('Repository name'),
    '#description' => t('A label for the repository that will be used in all user visible messages.'),
    '#default_value' => $repository_exists ? check_plain($repository['name']) : '',
    '#required' => TRUE,
    '#weight' => 0,
    '#size' => 40,
    '#maxlength' => 255,
  );
  $form['repository_information']['root'] = array(
    '#type' => 'textfield',
    '#title' => t('Repository root'),
    '#description' => t('The location of the repository\'s root directory.'),
    '#default_value' => $repository_exists ? $repository['root'] : '',
    '#weight' => 5,
    '#size' => 60,
    '#maxlength' => 255,
  );

  $result = db_query('SELECT registration_message
                      FROM {versioncontrol_repository_metadata}
                      WHERE repo_id = %d', $repo_id);
  while ($row = db_fetch_object($result)) {
    $current_registration_message = $row->registration_message;
  }
  if (!isset($current_registration_message)) {
    $presets = _versioncontrol_get_string_presets();
    $current_registration_message = $presets['versioncontrol_registration_message_repository'];
  }

  $form['account_registration'] = array(
    '#type' => 'fieldset',
    '#title' => t('Account registration'),
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
    '#weight' => 1,
  );
  $form['account_registration']['authorization_method'] = array(
    '#type' => 'select',
    '#title' => t('Authorization method'),
    '#description' => t('Determines the way that users can register an account for this repository.'),
    '#options' => versioncontrol_get_authorization_methods(),
    '#default_value' => $repository_exists
                        ? $repository['authorization_method']
                        : _versioncontrol_get_fallback_authorization_method(),
    '#weight' => 0,
  );
  $form['account_registration']['repository_messages'] = array(
    '#type' => 'fieldset',
    '#title' => t('Account registration form strings'),
    '#description' => t('The following message is shown on the !account-registration-page for this repository.', array('!account-registration-page' => l(t('account registration page'), 'versioncontrol/register/demo/'. $repo_id))),
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
    '#weight' => 2,
  );
  $form['account_registration']['repository_messages']['registration_message'] = array(
    '#type' => 'textarea',
    '#title' => t('Message to registering users'),
    '#description' => t('Message to show to users that are in the process of creating an account in this repository.'),
    '#default_value' => $current_registration_message,
  );

  if ($repository_exists) {
    $repo_urls = _versioncontrol_get_repository_urls($repository);
  }

  // TODO: abstract out repository URLs into separate backends
  $form['repository_urls'] = array(
    '#type' => 'fieldset',
    '#title' => t('Repository browser URLs'),
    '#description' =>  t('These URLs will be used to add links to item and commit displays such as the commit log.'),
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
    '#weight' => 10,
  );
  $form['repository_urls']['commit_view'] = array(
    '#type' => 'textfield',
    '#title' => t('Commit view URL'),
    '#default_value' => isset($repo_urls) ? $repo_urls['commit_view'] : '',
    '#size' => 40,
    '#maxlength' => 255,
    '#description' => t('Enter URL to the commit view web site. Use the %revision variable in the right place to represent the global revision identifier of the commit.'),
  );
  $form['repository_urls']['file_log_view'] = array(
    '#type' => 'textfield',
    '#title' => t('File log URL'),
    '#default_value' => isset($repo_urls) ? $repo_urls['file_log_view'] : '',
    '#size' => 40,
    '#maxlength' => 255,
    '#description' => t('Enter URL to the log view of a file in the repository. Use the %path, %revision and %branch variables in the right place to represent the path, revision and branch of the originating version of the file for the log.'),
  );
  $form['repository_urls']['file_view'] = array(
    '#type' => 'textfield',
    '#title' => t('File content URL'),
    '#default_value' => isset($repo_urls) ? $repo_urls['file_view'] : '',
    '#size' => 40,
    '#maxlength' => 255,
    '#description' => t('Enter URL to the content view of a file in the repository. Use the %path, %revision and %branch variables in the right place to represent the path, revision and branch of the file.'),
  );
  $form['repository_urls']['directory_view'] = array(
    '#type' => 'textfield',
    '#title' => t('Directory listing URL'),
    '#default_value' => isset($repo_urls) ? $repo_urls['directory_view'] : '',
    '#size' => 40,
    '#maxlength' => 255,
    '#description' => t('Enter URL to the file listing of a directory in the repository. Use the %path, %revision and %branch variables in the right place to represent the path, revision and branch of the file.'),
  );
  $form['repository_urls']['diff'] = array(
    '#type' => 'textfield',
    '#title' => t('Diff URL'),
    '#default_value' => isset($repo_urls) ? $repo_urls['diff'] : '',
    '#size' => 40,
    '#maxlength' => 255,
    '#description' => t('Enter URL to the diff web site. Use the %path, %new-revision, %old-revision and %branch variables in the right place to represent the file path, old revision and new revision. If the tool supports diffs between wholly different files, you can also use %old-path for the path of the old version of the file.'),
  );
  $form['repository_urls']['tracker'] = array('#type' => 'textfield',
    '#title' => t('Issue tracker URL'),
    '#default_value' => isset($repo_urls) ? $repo_urls['tracker'] : '',
    '#size' => 40,
    '#maxlength' => 255,
    '#description' => t('Enter URL to link to issues in log messages. Use the %d variable in the right place to represent the issue/case/bug id.'),
  );

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save repository'),
    '#weight' => 100,
  );

  return $form;
}

/**
 * Validate the add/edit repository form before it is submitted.
 */
function versioncontrol_admin_repository_edit_validate($form, &$form_state) {
  if ($form_state['values']['repo_id'] != 0 && $form_state['values']['repo_name'] == $form_state['values']['original_name']) {
    return;
  }
  $same_repositories = versioncontrol_get_repositories(array(
    'names' => array($form_state['values']['repo_name']),
  ));
  if (!empty($same_repositories)) {
    form_set_error('repo_name',
      t('The repository name %repo-name is already in use, please assign a different one.',
        array('%repo-name' => $form_state['values']['repo_name']))
    );
  }
}

/**
 * Add or update the repository when the add/edit form is submitted.
 */
function versioncontrol_admin_repository_edit_submit($form, &$form_state) {
  // Reconstruct the repository from the form values that were passed.
  $repository = array();
  $repository_urls = array();
  $repository_url_keys = array(
    'commit_view', 'file_log_view', 'file_view', 'directory_view',
    'diff', 'tracker'
  );

  foreach ($form_state['values'] as $key => $value) {
    if (in_array($key, array('repo_id', 'vcs', 'root', 'authorization_method'))) {
      $repository[$key] = $form_state['values'][$key];
    }
    else if ($key == 'repo_name') {
      $repository['name'] = $form_state['values'][$key];
    }
    else if (in_array($key, $repository_url_keys)) {
      $repository_urls[$key] = $form_state['values'][$key];
    }
    else if ($key == 'registration_message') {
      $registration_message = $form_state['values'][$key];
    }
  }
  $repository['url_backend'] = 'versioncontrol_default_urls'; // hardcoded for now

  $additional_data = module_invoke_all(
    'versioncontrol_extract_repository_data', $form_state['values']
  );
  $repository = array_merge($additional_data, $repository);

  if ($repository['repo_id'] > 0) {
    versioncontrol_update_repository($repository, $repository_urls);
    db_query("UPDATE {versioncontrol_repository_metadata}
              SET registration_message = '%s'
              WHERE repo_id = %d",
              $registration_message, $repository['repo_id']);

    drupal_set_message(t('The %repository repository has been updated.', array(
      '%repository' => $repository['name'],
    )));
  }
  else {
    $repository = versioncontrol_insert_repository($repository, $repository_urls);
    db_query("INSERT INTO {versioncontrol_repository_metadata}
              (repo_id, registration_message)
              VALUES (%d, '%s')",
              $repository['repo_id'], $registration_message);

    drupal_set_message(t('The %repository repository has been added.', array(
      '%repository' => $repository['name']
    )));
  }

  $form_state['redirect'] = 'admin/project/versioncontrol-repositories';
}


/**
 * Form callback for 'admin/project/versioncontrol-repositories/delete/%versioncontrol_repository':
 * Provide a form to confirm deletion of a repository.
 */
function versioncontrol_admin_repository_delete_confirm(&$form_state, $repository) {
  $form = array();
  $form['repo_id'] = array('#type' => 'value', '#value' => $repo_id);

  $form = confirm_form($form,
    t('Are you sure you want to delete %name?', array('%name' => $repository['name'])),
    $_GET['destination'] ? $_GET['destination'] : 'admin/project/versioncontrol-repositories',
    t('Mind that by deleting the repository, all associated data such as commits and account associations will be deleted as well.'),
    t('Delete'), t('Cancel')
  );
  return $form;
}

/**
 * Delete the repository when the confirmation form is submitted.
 */
function versioncontrol_admin_repository_delete_confirm_submit($form, &$form_state) {
  $repository = versioncontrol_get_repository($form_state['values']['repo_id']);
  versioncontrol_delete_repository($repository);
  drupal_set_message(t('The %repository repository has been deleted.', array(
    '%repository' => $repository['name'],
  )));
  $form_state['redirect'] = 'admin/project/versioncontrol-repositories';
}
