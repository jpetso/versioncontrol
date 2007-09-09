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
 * Copyright 2007 by Jakob Petsovits ("jpetso", http://drupal.org/user/56020)
 */

/**
 * Form callback for "admin/project/versioncontrol-settings/general":
 * Provide a form for settings of Version Control API itself.
 */
function versioncontrol_admin_settings() {
  $form = array();
  $options = array_merge(
    array('none' => t('No approval required')),
    versioncontrol_get_authorization_methods()
  );
  $presets = _versioncontrol_get_admin_presets();

  $form['#id'] = 'versioncontrol-settings-form';

  $form['versioncontrol_email_address'] = array(
    '#type' => 'textfield',
    '#title' => t('E-mail address'),
    '#default_value' => variable_get('versioncontrol_email_address', 'versioncontrol@example.com'),
    '#description' => t('The e-mail address of the VCS administrator.'),
    '#weight' => -10,
  );

  $form['versioncontrol_authorization_method'] = array(
    '#type' => 'select',
    '#title' => t('Account authorization mode'),
    '#description' => t('Determines the way that users gain access to VCS repositories.'),
    '#options' => $options,
    '#default_value' => versioncontrol_get_current_authorization_method(),
    '#weight' => -2,
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
function versioncontrol_admin_settings_submit($form_id, $form_values) {
  $value_names = array(
    'versioncontrol_email_address',
    'versioncontrol_authorization_method',
    'versioncontrol_allow_unauthorized_access',
    'versioncontrol_registration_message_unauthorized',
    'versioncontrol_registration_message_authorized',
  );
  foreach ($value_names as $name) {
    variable_set($name, $form_values[$name]);
  }
}


/**
 * Form callback for "admin/project/versioncontrol-accounts":
 * A list of accounts with filtering possibilities.
 */
function versioncontrol_admin_account_list_page() {
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

  $uid_constraints = array();
  $params = array();
  foreach ($users as $uid => $user) {
    $uid_constraints[] = "uid = '%d'";
    $params[] = $uid;
  }

  // Retrieve total, first and last commits of each user.
  $result = db_query("SELECT uid, COUNT(commit_id) AS number_commits,
                       MIN(date) AS first_commit_date, MAX(date) AS last_commit_date,
                       MIN(commit_id) AS first_commit_id, MAX(commit_id) AS last_commit_id
                      FROM {versioncontrol_commits}
                      WHERE ". implode(' OR ', $uid_constraints) ."
                      GROUP BY uid", $params);

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
        theme('commitlog_commit_id', $statistics->first_commit_id, $first_commits[$statistics->uid]);
      $last_commits[$statistics->uid] =
        theme('commitlog_commit_id', $statistics->last_commit_id, $last_commits[$statistics->uid]);
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
      if (module_exists('commitlog')) {
        $username = theme('commitlog_account_username',
                          $uid, $username, $repositories[$repo_id], FALSE);
        $repo_name = theme('commitlog_repository', $repositories[$repo_id]);
      }
      else {
        $username = check_plain($username);
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
function versioncontrol_admin_account_list_filter_form($repositories) {
  $form = array();
  $repository_names = array(0 => t('All'));

  foreach ($repositories as $repo_id => $repository) {
    $repository_names[$repo_id] = check_plain($repository['name']);
  }
  if (!isset($_SESSION['versioncontrol_filter_repo_id'])) {
    $_SESSION['versioncontrol_filter_repo_id'] = 0;
  }

  $form['#id'] = 'vcs-account-filter-form';

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
function versioncontrol_admin_account_list_filter_form_submit($form_id, $form_values) {
  $_SESSION['versioncontrol_filter_repo_id'] = $form_values['repo_id'];
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
function versioncontrol_admin_account_import_form() {
  $backends = versioncontrol_get_backends();
  $selected_backends = array();
  foreach ($backends as $vcs => $backend) {
    if (versioncontrol_backend_implements($vcs, 'import_accounts')) {
      $selected_backends[] = $vcs;
    }
  }

  $repositories = versioncontrol_get_repositories(array('vcs' => $selected_backends));
  $form = array();

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
 * Submit handler for the "select repository to import" form from above:
 * Call the [vcs_backend]_admin_import_accounts() with the given data.
 */
function versioncontrol_admin_account_import_form_submit($form_id, $form_values) {
  $repository = versioncontrol_get_repository($form_values['repo_id']);
  if (!isset($repository)) {
    drupal_set_message(t('The selected repository could not be found.'), 'error');
    return;
  }
  _versioncontrol_call_backend($repository['vcs'], 'import_accounts',
                               array($repository, $form_values['data']));
}


/**
 * Callback for "admin/project/versioncontrol-accounts/export":
 * Export a repository's authenticated accounts to the version control system's
 * password file format.
 */
function versioncontrol_admin_account_export_page() {
  $repo_id = arg(4);

  if (is_numeric($repo_id)) {
    $repository = versioncontrol_get_repository($repo_id);
  }
  if (!isset($repository)) {
    // When the repository is not given, show the redirection page.
    return drupal_get_form('versioncontrol_admin_account_export_form');
  }
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
    return drupal_get_form('versioncontrol_admin_account_export_display_form',
                           $repository, $data);
  }
}

/**
 * Form callback for "admin/project/versioncontrol-accounts/export":
 * Select the repository from which to extract data, and show the exported
 * data on submitting the form.
 */
function versioncontrol_admin_account_export_form() {
  $backends = versioncontrol_get_backends();
  $selected_backends = array();
  foreach ($backends as $vcs => $backend) {
    if (versioncontrol_backend_implements($vcs, 'export_accounts')) {
      $selected_backends[] = $vcs;
    }
  }

  $repositories = versioncontrol_get_repositories(array('vcs' => $selected_backends));
  $form = array();

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
function versioncontrol_admin_account_export_form_submit($form_id, $form_values) {
  $target_path = 'admin/project/versioncontrol-accounts/export/'. $form_values['repo_id'];
  if ($form_values['op'] == t('Export to plaintext')) {
    $target_path .= '/plaintext';
  }
  return $target_path;
}

/**
 * Form callback for "admin/project/versioncontrol-accounts/export/$repo_id":
 * Show the exported account data in a form as text area.
 *
 * @param $repository
 *   The repository for which accounts have been exported.
 * @param $data
 *   The exported account data - the text that is supposed to be copied
 *   to the version control system's accounts file.
 */
function versioncontrol_admin_account_export_display_form($repository, $data) {
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
 * Form callback for "versioncontrol/register":
 * Provide an indirection that leads to an account registration form.
 */
function versioncontrol_account_register_page() {
  global $user;
  $presets = _versioncontrol_get_admin_presets();
  $form = array();

  if (arg(2) == 'demo') { // demo mode for showing the page to the admin
    $uid = -1;
  }
  else {
    $uid = is_numeric(arg(2)) ? arg(2) : $user->uid;
  }
  $user_access = user_access('use version control systems');
  $admin_access = user_access('administer version control systems');

  if (!$user_access || $user->uid == 0) {
    return variable_get(
      'versioncontrol_registration_message_unauthorized',
      $presets['versioncontrol_registration_message_unauthorized']
    );
  }
  if (!$admin_access && $uid != $user->uid) {
    drupal_access_denied();
    return;
  }

  $accounts = versioncontrol_get_accounts(array('uids' => array($uid)), TRUE);
  $repositories = versioncontrol_get_repositories();

  // Construct the '#options' array.
  $account_repo_ids = array();
  foreach ($accounts as $uid => $usernames_by_repository) {
    foreach ($usernames_by_repository as $repo_id => $username) {
      $account_repo_ids[] = $repo_id;
    }
  }

  if (arg(2) == 'demo') { // demo mode for showing the page to the admin
    $account_repo_ids = array();
  }

  $repository_names = array();
  foreach ($repositories as $repo_id => $repository) {
    if (in_array($repo_id, $account_repo_ids)) {
      continue; // We only want repositories shown where no account exists yet.
    }
    if (!isset($first_repo_id)) {
      $first_repo_id = $repo_id;
    }
    $repository_names[$repo_id] = check_plain($repository['name']);
  }

  // If there's just one repository on the site, redirect directly to this one.
  if (count($repository_names) == 1) {
    $only_repo_id = $first_repo_id;
  }
  else if (count($repositories) == 1) {
    foreach ($repositories as $repo_id => $repository) {
      $only_repo_id = $repo_id;
    }
  }

  if (arg(2) == 'demo') { // demo mode for showing the page to the admin
    unset($only_repo_id);
  }

  if (is_numeric(arg(3)) || isset($only_repo_id)) {
    $repo_id = isset($only_repo_id) ? $only_repo_id : arg(3);
    if (in_array($repo_id, $account_repo_ids)) {
      drupal_set_message(t('You already have a registered account.'));
      drupal_goto('user/'. $user->uid .'/edit/versioncontrol/'. $repo_id);
      return '';
    }
    else if (isset($repositories[$repo_id])) {
      return drupal_get_form('versioncontrol_account_edit_form',
                             $uid, '', $repositories[$repo_id]);
    }
  }

  return drupal_get_form('versioncontrol_account_register_form',
                         $uid, $repository_names, $first_repo_id);
}

/**
 * The actual form for "versioncontrol/register".
 */
function versioncontrol_account_register_form($uid, $repository_names, $first_repo_id) {
  if (empty($repository_names)) {
    drupal_set_title(t('No more registrations possible'));
    $form['no_registration'] = array(
      '#type' => 'markup',
      '#value' => t('You already have an account for all the repositories known to this site. Go to your !user-account-page to configure repository account settings.',
        array('!user-account-page' => l(t('user account page'), 'user/'. $uid .'/edit'))
      ),
      '#prefix' => '<p>',
      '#suffix' => '</p>',
    );
    return $form;
  }

  $form['#id'] = 'vcs-account-indirection-form';

  $message = variable_get(
    'versioncontrol_registration_message_authorized',
    $presets['versioncontrol_registration_message_authorized']
  );
  if (!empty($message)) {
    $form['overview'] = array(
      '#type' => 'fieldset',
      '#title' => t('Overview'),
      '#weight' => -100,
    );
    $form['overview']['overview'] = array(
      '#type' => 'markup',
      '#value' => $message,
    );
  }

  $form['uid'] = array(
    '#type' => 'value',
    '#value' => $uid,
  );

  $form['repository'] = array(
    '#type' => 'fieldset',
    '#title' => t('Select repository'),
    '#weight' => 10,
  );
  $form['repository']['repo_id'] = array(
    '#type' => 'select',
    '#title' => t('Create user account in'),
    '#options' => $repository_names,
    '#default_value' => $first_repo_id,
  );

  if (arg(2) != 'demo') { // demo mode for showing the page to the admin
    $form['repository']['submit'] = array(
      '#type' => 'submit',
      '#value' => t('Create account'),
      '#weight' => 100,
    );
  }
  return $form;
}

/**
 * Submit handler for the indirection form.
 * Surprisingly, all it does is redirect to the appropriate registration form.
 */
function versioncontrol_account_register_form_submit($form_id, $form_values) {
  return 'versioncontrol/register/'. $form_values['uid'] .'/'. $form_values['repo_id'];
}


/**
 * Form callback for "user/$uid/edit/versioncontrol/$repository[repo_id]",
 * "versioncontrol/register/$uid/$repository[repo_id]" and (in certain cases)
 * "versioncontrol/register/$uid" or just "versioncontrol/register":
 * Provide a form to edit or register a VCS user account.
 *
 * @param $uid
 *   The Drupal user id of the user whose account is being edited.
 * @param $username
 *   The VCS specific username of that user. If this is an empty string,
 *   it means that a user account for this repository does not yet exist
 *   and should be created.
 * @param $repository
 *   The repository of the added/edited account.
 */
function versioncontrol_account_edit_form($uid, $username, $repository) {
  $form = array();
  $is_new_account = empty($username);
  $backends = versioncontrol_get_backends();
  $vcs_name = $backends[$repository['vcs']]['name'];
  $admin_access = user_access('administer version control systems');

  $form['#id'] = 'vcs-account-form';
  $form['#repository'] = $repository;
  $form['#vcs'] = $repository['vcs'];
  $form['#validate'] = array('versioncontrol_account_edit_form_validate' => array());
  $form['#submit'] = array('versioncontrol_account_edit_form_submit' => array());

  // Values stored for later processing.
  $form['uid'] = array(
    '#type' => 'value',
    '#value' => $uid,
  );
  $form['original_username'] = array(
    '#type' => 'value',
    '#value' => $username,
  );
  $form['repo_id'] = array(
    '#type' => 'value',
    '#value' => $repository['repo_id'],
  );
  $form['vcs_name'] = array(
    '#type' => 'value',
    '#value' => $vcs_name,
  );

  if ($is_new_account) {
    $result = db_query("SELECT registration_message
                        FROM {versioncontrol_repository_internal_data}
                        WHERE repo_id = '%d'", $repository['repo_id']);
    while ($row = db_fetch_object($result)) {
      $registration_message = $row->registration_message;
    }
    if (!empty($registration_message)) {
      $form['overview'] = array(
        '#type' => 'fieldset',
        '#title' => t('Overview'),
        '#weight' => -100,
      );
      $form['overview']['overview'] = array(
        '#type' => 'markup',
        '#value' => $registration_message,
      );
    }
  }

  $form['account'] = array(
    '#type' => 'fieldset',
    '#title' => t('@vcs account settings', array('@vcs' => $vcs_name)),
    '#weight' => 0,
  );

  if ($is_new_account || $admin_access) {
    $user = user_load(array('uid' => $uid));

    // Set a more appropriate title for account registrations.
    if ($is_new_account) {
      drupal_set_title(t(
        'Create user account in @repository',
        array('@repository' => $repository['name'])
      ));

      // Have a nice default value for the new VCS username.
      $username = strtr(strtolower($user->name),
        array(' ' => '', '@' => '', '.' => '', '-' => '', '_' => '', '.' => '')
      );
    }
    else { // For the tab on the user edit page:
      // When someone else than the user goes here (the admin, that is),
      // the page title is incorrect and would say 'User', which is bad.
      drupal_set_title(check_plain($user->name));
    }

    if ($admin_access) { // the admin version
      $description = t('The @vcs username associated with the account of !user. This field is used to link commit messages to user accounts.', array('@vcs' => $vcs_name, '!user' => theme_username($user)));
    }
    else { // the account creation version
      $description = t('Choose a username to access the @vcs repository with. @vcs usernames should be lowercase. Spaces or other special characters are not allowed.', array('@vcs' => $vcs_name));
    }

    $form['account']['account_name'] = array(
      '#type' => 'textfield',
      '#title' => t('@vcs username', array('@vcs' => $vcs_name)),
      '#description' => $description,
      '#default_value' => $username,
      '#weight' => 0,
      '#size' => 30,
      '#maxlength' => 64,
    );
  }
  else {
    $form['account_name'] = array(
      '#type' => 'value',
      '#value' => $username,
    );
    $form['account']['account_name_display'] = array(
      '#type' => 'item',
      '#title' => t('@vcs username', array('@vcs' => $vcs_name)),
      '#description' => t('Your @vcs username. This field can only be edited by administrators and is used to link your @vcs messages to your user account.', array('@vcs' => $vcs_name)),
      '#value' => $username,
      '#weight' => 0,
    );
  }

  if (arg(2) != 'demo') { // demo mode for showing the page to the admin
    $form['submit'] = array(
      '#type' => 'submit',
      '#value' => $is_new_account
                  ? t('Create @vcs account', array('@vcs' => $vcs_name))
                  : t('Update @vcs account', array('@vcs' => $vcs_name)),
      '#weight' => 100,
    );
  }
  return $form;
}

/**
 * Validate the edit/register user account form submission before it is submitted.
 */
function versioncontrol_account_edit_form_validate($form_id, $form_values) {
  if (!isset($form_values['account_name'])) {
    return;
  }
  $uid = $form_values['uid'];
  $username = trim($form_values['account_name']);
  $repository = versioncontrol_get_repository($form_values['repo_id']);
  $vcs_name = $form_values['vcs_name'];

  if (!isset($repository)) { // admin deletes repo while user fills in form
    form_set_error('account',
      t('The repository has been deleted.', array('@vcs' => $vcs_name))
    );
    return;
  }

  if (empty($username)) {
    form_set_error('account_name',
      t('The @vcs username may not be empty.', array('@vcs' => $vcs_name))
    );
  }
  else {
    // Check for unallowed characters. (Only alphanumeric ones are allowed.)
    if (ereg("[^[:alnum:]]", $username)) {
      form_set_error('account_name',
        t('The specified @vcs username is invalid.', array('@vcs' => $vcs_name))
      );
    }
    // Check for duplicates.
    $existing_uid = versioncontrol_get_account_uid_for_username($repository['repo_id'], $username, TRUE);
    if ($existing_uid && $uid != $existing_uid) {
      if ($existing_user = user_load(array('uid' => $existing_uid))) {
        $existing_username = theme('username', $existing_user);
      }
      else {
        $existing_username = t('user #!id', array('!id' => $existing_uid));
      }
      form_set_error('account_name',
        t('The specified @vcs username is already in use by !existing-user.',
          array('@vcs' => $vcs_name, '!existing-user' => $existing_username))
      );
    }
  }
}

/**
 * Add or update the user account when the edit/register form is submitted.
 */
function versioncontrol_account_edit_form_submit($form_id, $form_values) {
  // Reconstruct the user data from the $form_values that were passed.
  $uid = $form_values['uid'];
  $username = trim($form_values['account_name']);
  $repository = versioncontrol_get_repository($form_values['repo_id']);
  $vcs_name = $form_values['vcs_name'];
  $vcs_specific = NULL;

  $additional_data = module_invoke_all(
    'versioncontrol_extract_account_data', $form_values
  );

  if (empty($form_values['original_username'])) {
    versioncontrol_insert_account($repository, $uid, $username, $additional_data);
    $message = drupal_set_message(t(
      'The @vcs account %username has been registered.',
      array('@vcs' => $vcs_name, '%username' => $username)
    ));
  }
  else {
    versioncontrol_update_account($repository, $uid, $username, $additional_data);

    // Regular users cannot change the username, and will probably get
    // a message for each of the other actions that hook into the form.
    if (user_access('administer version control systems')) {
      $message = drupal_set_message(t(
        'The @vcs account %username has been updated successfully.',
        array('@vcs' => $vcs_name, '%username' => $username)
      ));
    }
  }
  if (user_access('administer version control systems')) {
    return 'admin/project/versioncontrol-accounts';
  }
  else {
    return 'user/'. $uid .'/edit/versioncontrol/'. $repository['repo_id'];
  }
}


/**
 * Form callback for "admin/project/versioncontrol-repositories":
 * A simple list of repositories, sorted by version control system.
 */
function versioncontrol_admin_repository_list() {
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
    $form[$vcs] = array(
      '#value' => '<h4>'. $backends[$vcs]['name'] .' repositories</h4>',
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
      '#value' => theme('table', $header, $rows)
    );
  }
  return $form;
}

/**
 * Form callback for "admin/project/versioncontrol-repositories/edit/$repo_id"
 * and "admin/project/versioncontrol-repositories/add-$vcs":
 * Provide a form to edit or add a repository.
 *
 * @param $repo_id
 *   The repository id of the repository that is to be edited,
 *   or 0 if the repository doesn't exist yet and should be added.
 * @param $vcs
 *   If $repo_id is 0, this should be the unique identification string
 *   of the backend for which the repository should be added.
 *   Otherwise, this needs to be NULL.
 */
function versioncontrol_admin_repository_edit($repo_id, $vcs = NULL) {
  $backends = versioncontrol_get_backends();

  if (!$repo_id) {
    if (!isset($backends[$vcs])) {
      drupal_goto('admin/project/versioncontrol-repositories');
      return array();
    }
  }
  else {
    $repository = versioncontrol_get_repository($repo_id);
    if (!isset($repository)) {
      drupal_goto('admin/project/versioncontrol-repositories');
      return array();
    }
  }
  $vcs = isset($repository) ? $repository['vcs'] : $vcs;

  $form = array();
  $form['#id'] = 'vcs-repository-form';
  $form['#vcs'] = $vcs;
  $form['#repository'] = $repository;

  $form['repo_id'] = array(
    '#type' => 'value',
    '#value' => isset($repository) ? $repository['repo_id'] : 0,
  );
  $form['original_name'] = array(
    '#type' => 'value',
    '#value' => isset($repository) ? check_plain($repository['name']) : 0,
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
    '#default_value' => isset($repository) ? check_plain($repository['name']) : '',
    '#weight' => 0,
    '#size' => 40,
    '#maxlength' => 255,
  );
  $form['repository_information']['root'] = array(
    '#type' => 'textfield',
    '#title' => t('Repository root'),
    '#default_value' => isset($repository) ? $repository['root'] : '',
    '#weight' => 5,
    '#size' => 60,
    '#maxlength' => 255,
    '#description' => t('The location of the repository\'s root directory.'),
  );

  $result = db_query("SELECT registration_message
                      FROM {versioncontrol_repository_internal_data}
                      WHERE repo_id = '%d'", $repo_id);
  while ($row = db_fetch_object($result)) {
    $current_registration_message = $row->registration_message;
  }
  if (!isset($current_registration_message)) {
    $presets = _versioncontrol_get_admin_presets();
    $current_registration_message = $presets['versioncontrol_registration_message_repository'];
  }

  $form['repository_messages'] = array(
    '#type' => 'fieldset',
    '#title' => t('Account registration form strings'),
    '#description' => t('The following message is shown on the !account-registration-page for this repository.', array('!account-registration-page' => l(t('account registration page'), 'versioncontrol/register/demo/'. $repo_id))),
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
    '#weight' => 2,
  );
  $form['repository_messages']['registration_message'] = array(
    '#type' => 'textarea',
    '#title' => t('Message to registering users'),
    '#description' => t('Message to show to users that are in the process of creating an account in this repository.'),
    '#default_value' => $current_registration_message,
  );

  if (isset($repository)) {
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
  $form['repository_urls']['file_view'] = array(
    '#type' => 'textfield',
    '#title' => t('File view URL'),
    '#default_value' => isset($repo_urls) ? $repo_urls['file_view'] : '',
    '#size' => 40,
    '#maxlength' => 255,
    '#description' => t('Enter URL to the display view of a file in the repository. Use the %path, %revision and %branch variables in the right place to represent the path, revision and branch of the file.'),
  );
  $form['repository_urls']['directory_view'] = array(
    '#type' => 'textfield',
    '#title' => t('Directory view URL'),
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
function versioncontrol_admin_repository_edit_validate($form_id, $form_values) {
  if ($form_values['repo_id'] != 0 && $form_values['repo_name'] == $form_values['original_name']) {
    return;
  }
  $same_repositories = versioncontrol_get_repositories(array(
    'names' => array($form_values['repo_name']),
  ));
  if (!empty($same_repositories)) {
    form_set_error('repo_name',
      t('The repository name %repo-name is already in use, please assign a different one.',
        array('%repo-name' => $form_values['repo_name']))
    );
  }
}

/**
 * Add or update the repository when the add/edit form is submitted.
 */
function versioncontrol_admin_repository_edit_submit($form_id, $form_values) {
  // Reconstruct the repository from the $form_values that were passed.
  $repository = array();
  $repository_urls = array();

  foreach ($form_values as $key => $value) {
    if (in_array($key, array('repo_id', 'vcs', 'root'))) {
      $repository[$key] = $form_values[$key];
    }
    else if ($key == 'repo_name') {
      $repository['name'] = $form_values[$key];
    }
    else if (in_array($key, array('commit_view', 'file_view', 'directory_view', 'diff', 'tracker'))) {
      $repository_urls[$key] = $form_values[$key];
    }
    else if ($key == 'registration_message') {
      $registration_message = $form_values[$key];
    }
  }
  $repository['url_backend'] = 'versioncontrol_default_urls'; // hardcoded for now

  $additional_data = module_invoke_all(
    'versioncontrol_extract_repository_data', $form_values
  );
  $repository = array_merge($additional_data, $repository);

  if ($repository['repo_id'] > 0) {
    versioncontrol_update_repository($repository, $repository_urls);
    db_query("UPDATE {versioncontrol_repository_internal_data}
              SET registration_message = '%s'
              WHERE repo_id = '%d'",
              $registration_message, $repository['repo_id']);

    drupal_set_message(t('The %repository repository has been updated.',
                       array('%repository' => $repository['name'])));
  }
  else {
    $repository = versioncontrol_insert_repository($repository, $repository_urls);
    db_query("INSERT INTO {versioncontrol_repository_internal_data}
              (repo_id, registration_message)
              VALUES ('%d', '%s')",
              $repository['repo_id'], $registration_message);

    drupal_set_message(t('The %repository repository has been added.',
                       array('%repository' => $repository['name'])));
  }

  return 'admin/project/versioncontrol-repositories';
}


/**
 * Form callback for "admin/project/versioncontrol-repositories/delete/$repo_id":
 * Provide a form to confirm deletion of a repository.
 */
function versioncontrol_admin_repository_delete_confirm($repo_id) {
  $repository = versioncontrol_get_repository($repo_id);

  if (!isset($repository)) {
    drupal_goto('admin/project/versioncontrol-repositories');
    return array();
  }

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
function versioncontrol_admin_repository_delete_confirm_submit($form_id, $form_values) {
  $repository = versioncontrol_get_repository($form_values['repo_id']);
  versioncontrol_delete_repository($repository);
  drupal_set_message(t('The %repository repository has been deleted.',
                      array('%repository' => $repository['name'])));

  drupal_goto('admin/project/versioncontrol-repositories');
}


/**
 * Return preset values that are used in the administration forms.
 */
function _versioncontrol_get_admin_presets() {
  $presets = array();

  $presets['versioncontrol_registration_message_unauthorized'] = t('<p>The Concurrent Versioning System (CVS) is a software development tool available to volunteers with experience in software development, translation, theming, or documentation who wish to participate in the Drupal project.</p>
<p>To request access to the Drupal CVS repository you must <a href="!register">create an account</a> and <a href="!login">login</a>. Come back to this page after you have logged on.</p>',
    array('!register' => url('user/register'), '!login' => url('user/login'))
  );

  $presets['versioncontrol_registration_message_authorized'] = t('<p>The Concurrent Versioning System (CVS) is a software development tool available to volunteers with experience in software development, translation, theming, or documentation who wish to participate in the Drupal project.</p>
<p>A version control system account is not required to contribute patches to the Drupal project or community contributed projects. Anonymous access to the Drupal CVS repository is available which can be used to accomplish this. Please peruse the <a href="!handbook">CVS handbook</a> and <a href="!patch">patch guide</a> for more information.</p>
<p>If you are an open source software developer, themer, translator or documentation writer, please choose one of Drupal\'s repositories from the selection below in order to request commit access to this repository. Prior to applying, please ensure that:</p>
<ul>
  <li>you have a concrete idea as to what you intend to contribute.</li>
  <li>you are not duplicating any <a href="!projects">existing projects</a>.</li>
</ul>',
    array('!projects' => url('project'), '!handbook' => url('handbook/cvs'), '!patch' => url('patch'))
  );

  $presets['versioncontrol_registration_message_repository'] = t('<p>The Concurrent Versioning System (CVS) is a software development tool available to volunteers with experience in software development, translation, theming, or documentation who wish to participate in the Drupal project.</p>
<p>A version control system account is not required to contribute patches to the Drupal project or community contributed projects. Anonymous access to the Drupal CVS repository is available which can be used to accomplish this. Please peruse the <a href="!handbook">CVS handbook</a> and <a href="!patch">patch guide</a> for more information.</p>
<p>If you are an open source software developer, themer, translator or documentation writer, please use the form below to create an account in Drupal\'s CVS repository. Prior to registering, please ensure that:</p>
<ul>
  <li>you have a concrete idea as to what you intend to contribute.</li>
  <li>you are not duplicating any <a href="!projects">existing projects</a>.</li>
</ul>',
    array('!projects' => url('project'), '!handbook' => url('handbook/cvs'), '!patch' => url('patch'))
  );

  return $presets;
}
