<?php
// $Id$
/**
 * @file
 * Version Control API - An interface to version control systems
 * whose functionality is provided by pluggable back-end modules.
 *
 * This file contains the user interface for non-admin tasks.
 *
 * Copyright 2006, 2007 Derek Wright ("dww" , http://drupal.org/user/46549)
 * Copyright 2007, 2009 by Jakob Petsovits ("jpetso", http://drupal.org/user/56020)
 */

// The account registration form has a special demo mode to show the admin how
// account registration will look like in the general case, mainly in order to
// demonstrate how the various registration messages are being used.
define('VERSIONCONTROL_REGISTER_DEMO', -1);

/**
 * Form callback for "versioncontrol/register[/$register_uid[/$register_at_repo_id]]":
 * Provide an indirection that leads to an account registration form.
 */
function versioncontrol_account_register_page($register_uid = FALSE, $register_at_repo_id = FALSE) {
  global $user;

  if ($user->uid == 0 || !versioncontrol_user_access()) {
    $presets = _versioncontrol_get_string_presets();

    return variable_get(
      'versioncontrol_registration_message_unauthorized',
      $presets['versioncontrol_registration_message_unauthorized']
    );
  }

  if ($register_uid == 'demo') {
    $register_uid = VERSIONCONTROL_REGISTER_DEMO;
  }
  else {
    $register_uid = is_numeric($register_uid) ? $register_uid : $user->uid;
  }

  $admin_access = versioncontrol_admin_access();

  if (!$admin_access && $register_uid != $user->uid) {
    drupal_access_denied();
    return;
  }

  $accounts = versioncontrol_get_accounts(array('uids' => array($register_uid)), TRUE);
  $repositories = versioncontrol_get_repositories();

  // Construct the '#options' array.
  $account_usernames = array();
  foreach ($accounts as $uid => $usernames_by_repository) {
    foreach ($usernames_by_repository as $repo_id => $username) {
      $account_usernames[$repo_id][] = $username;
    }
  }

  if ($register_uid == VERSIONCONTROL_REGISTER_DEMO) {
    $account_usernames = array();
  }

  $repository_names = array();
  foreach ($repositories as $repo_id => $repository) {
    if (isset($account_usernames[$repo_id])
        && versioncontrol_is_account_authorized($repository, $register_uid)) {
      // We only want repositories in the list of repositories that are open
      // for registrations where no (authorized) account exists yet.
      continue;
    }
    if (!isset($first_repo_id)) {
      $first_repo_id = $repo_id;
    }
    $repository_names[$repo_id] = check_plain($repository['name']);
  }

  // Filter (and possibly change the caption of) the list of repositories to
  // select. The admin has all the permissions and gets the uncensored list.
  if (!$admin_access) {
    foreach (module_implements('versioncontrol_alter_repository_selection') as $module) {
      $function = $module .'_versioncontrol_alter_repository_selection';
      $function($repository_names, $repositories);
    }
  }

  // If there's just one repository on the site, redirect directly to this one.
  if (count($repository_names) == 1) {
    $only_repo_id = $first_repo_id;
  }
  else if (count($repositories) == 1) {
    $only_repo_id = reset(array_keys($repositories));
  }

  if ($register_uid == VERSIONCONTROL_REGISTER_DEMO) {
    unset($only_repo_id);
  }

  if (is_numeric($register_at_repo_id) || isset($only_repo_id)) {
    // If there is only one repository left to register, it doesn't matter
    // whether or not the URL contains a repository (and which one), we always
    // redirect to the remaining possible one. Otherwise, we try to register
    // at the given repository.
    $repo_id = isset($only_repo_id) ? $only_repo_id : $register_at_repo_id;

    if (isset($account_usernames[$repo_id])) {
      drupal_set_message(t('You already have a registered account.'));
      drupal_goto('user/'. $user->uid .'/edit/versioncontrol/'. $repo_id
                  .'/'. reset($account_usernames[$repo_id]));
      // drupal_goto() calls exit() so script execution ends right here
    }
    else if (isset($repository_names[$repo_id])) {
      return drupal_get_form('versioncontrol_account_edit_form',
        $register_uid, $repositories[$repo_id], VERSIONCONTROL_FORM_CREATE
      );
    }
  }

  return drupal_get_form('versioncontrol_account_register_form',
    $register_uid, $repository_names, $first_repo_id
  );
}

/**
 * Implementation of hook_versioncontrol_alter_repository_selection():
 * Alter the list of repositories that are available for user registration
 * and editing. This hook is called for all users except those with
 * "administer version control systems" permissions.
 *
 * @param $repository_names
 *   The list of repository names as it is shown in the select box
 *   at 'versioncontrol/register'. Array keys are the repository ids,
 *   and array elements are the captions in the select box.
 *   There's two things that can be done with this array:
 *   - Change (amend) the caption, in order to provide more information
 *     for the user. (E.g. note that an application is necessary.)
 *   - Unset any number of array elements. If you do so, the user will not
 *     be able to register a new account for this repository.
 * @param $repositories
 *   A list of repositories (with the repository ids as array keys) that
 *   includes at least all of the repositories that correspond to the
 *   repository ids of the @p $repository_names array.
 */
function versioncontrol_versioncontrol_alter_repository_selection(&$repository_names, $repositories) {
  foreach ($repository_names as $repo_id => $caption) {
    if ($repositories[$repo_id]['authorization_method'] == 'versioncontrol_admin') {
      unset($repository_names[$repo_id]);
    }
  }
}

/**
 * The actual form for "versioncontrol/register[/$register_uid]".
 */
function versioncontrol_account_register_form(&$form_state, $register_uid, $repository_names, $first_repo_id) {
  $form = array();

  if (empty($repository_names)) {
    drupal_set_title(t('No more registrations possible'));
    $form['no_registration'] = array(
      '#type' => 'markup',
      '#value' => t('You already have an account for all the repositories where you can register. Go to your !user-account-page to configure repository account settings.',
        array('!user-account-page' => l(t('user account page'), 'user/'. $register_uid .'/edit/versioncontrol'))
      ),
      '#prefix' => '<p>',
      '#suffix' => '</p>',
    );
    return $form;
  }

  $form['#id'] = 'vcs-account-indirection-form';

  $message = variable_get('versioncontrol_registration_message_authorized', FALSE);
  if ($message == FALSE) {
    $presets = _versioncontrol_get_string_presets();
    $message = $presets['versioncontrol_registration_message_authorized'];
  }
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

  $form['#uid'] = $register_uid;

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

  if ($register_uid != VERSIONCONTROL_REGISTER_DEMO) {
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
function versioncontrol_account_register_form_submit($form, &$form_state) {
  $form_state['redirect'] = 'versioncontrol/register/'. $form['#uid'] .'/'. $form_state['values']['repo_id'];
}


/**
 * Retrieve the message that shows up on the repository edit form when a
 * new user wants to register an account.
 *
 * Only call this function from within a form_alter() that modifies the
 * account edit form.
 *
 * Note: I'm not really satisfied with this being a public function,
 * but the Account Status module needs to display this message also for
 * reapplications, which is not included in the original form.
 * At least, it seems to be a better solution than exposing the
 * {versioncontrol_repository_metadata} table or even adding the message
 * to the {versioncontrol_repository} table. For now, at least.
 *
 * @param $repo_id
 *   The repository id where the a new account is registered.
 *
 * @return
 *   The registration message if it has been set, or an empty string otherwise.
 */
function versioncontrol_get_repository_registration_message($repo_id) {
  $result = db_query('SELECT registration_message
                      FROM {versioncontrol_repository_metadata}
                      WHERE repo_id = %d', $repo_id);
  while ($row = db_fetch_object($result)) {
    return $row->registration_message;
  }
  return '';
}

/**
 * Form callback for 'user/%versioncontrol_user_accounts/edit/versioncontrol':
 * Display a list of the given user's VCS accounts, or directly return the form
 * array from versioncontrol_account_edit_form() if only a single account
 * exists for that user (or if the account has been given in the URL).
 */
function versioncontrol_account_page($accounts, $url_repo_id = NULL, $url_username = NULL) {
  $selected_usernames = array();

  foreach ($accounts as $only_uid => $usernames_by_repository) {
    // The caller (menu system) ensures that there is only one uid.
    $uid = $only_uid;

    foreach ($usernames_by_repository as $repo_id => $username) {
      if (isset($url_repo_id) && $repo_id != $url_repo_id) {
        unset($accounts[$uid][$repo_id]);
        continue; // disregard repositories that don't match the URL constraints
      }
      $usernames = array($username);

      foreach ($usernames as $key => $username) {
        if (isset($url_username) && $username != $url_username) {
          unset($accounts[$uid][$repo_id]);
          continue; // disregard usernames that don't match the URL constraints
        }
        $any_repo_id = $repo_id;
        $selected_usernames[] = $username;
      }
    }
  }

  if (empty($selected_usernames)) {
    drupal_not_found();
    return;
  }
  elseif (count($selected_usernames) == 1) {
    $repository = versioncontrol_get_repository($any_repo_id);
    return drupal_get_form('versioncontrol_account_edit_form',
      $uid, $repository, reset($selected_usernames)
    );
  }
  else {
    return drupal_get_form('versioncontrol_account_list_form', $accounts);
  }
}

/**
 * Form callback for "admin/project/versioncontrol-accounts" and
 * (in certain cases) "user/%versioncontrol_user_accounts/edit/versioncontrol":
 * Show a list of VCS accounts to the admin or the user.
 *
 * @param $accounts
 *   The list of accounts to show, in versioncontrol_get_accounts() format.
 */
function versioncontrol_account_list_form(&$form_state, $accounts) {
  $form = array();
  $form['#id'] = 'versioncontrol-account-list-form';

  $repositories = versioncontrol_get_repositories(array(
    'repo_ids' => array_keys(reset($accounts)),
  ));

  $header = array(t('Repository'), t('Username'), '');
  $rows = array();

  foreach ($accounts as $uid => $usernames_by_repository) {
    foreach ($usernames_by_repository as $repo_id => $username) {
      if (!isset($repositories[$repo_id])) { // happens if the backend isn't loaded
        continue;
      }
      $usernames = array($username);
      $backend = versioncontrol_get_backend($repositories[$repo_id]);

      foreach ($usernames as $username) {
        $formatted_username = theme('versioncontrol_account_username',
          $uid, $username, $repositories[$repo_id],
          array('prefer_drupal_username' => FALSE)
        );
        $repo_name = module_exists('commitlog')
          ? theme('commitlog_repository', $repositories[$repo_id])
          : check_plain($repositories[$repo_id]['name']);

        $rows[] = array(
          $repo_name,
          $formatted_username,
          l(t('Edit @vcs account', array('@vcs' => $backend['name'])),
            'user/'. $uid .'/edit/versioncontrol/'. $repo_id .'/'. drupal_urlencode($username)
          ),
        );
      }
    }
  }

  $form['accounts'] = array(
    '#type' => 'markup',
    '#value' => theme('table', $header, $rows, array('style' => 'width: 50%;')),
  );
  return $form;
}

/**
 * Form callback for (in certain cases) "versioncontrol/register"
 * and "user/%versioncontrol_user_accounts/edit/versioncontrol":
 * Provide a form to register or edit a VCS account.
 *
 * @param $uid
 *   The uid of the Drupal user whose account is being edited or registered.
 * @param $repository
 *   The repository of the added/edited account.
 * @param $vcs_username
 *   The user's VCS-specific username for the repository,
 *   or VERSIONCONTROL_FORM_CREATE if a new VCS account should be registered.
 */
function versioncontrol_account_edit_form(&$form_state, $uid, $repository, $vcs_username) {
  $form = array();
  $create_account = ($vcs_username === VERSIONCONTROL_FORM_CREATE);
  $backends = versioncontrol_get_backends();
  $vcs_name = $backends[$repository['vcs']]['name'];

  $user = ($uid === VERSIONCONTROL_REGISTER_DEMO) ? FALSE : user_load($uid);

  if (!($user || ($uid === VERSIONCONTROL_REGISTER_DEMO && $create_account))) {
    drupal_not_found(); // cannot edit/register accounts for non-existing users
    return array();
  }

  // Set an appropriate page title.
  if ($create_account) {
    drupal_set_title(t(
      'Create user account in @repository',
      array('@repository' => $repository['name'])
    ));
  }
  elseif ($user) {
    drupal_set_title(check_plain($user->name));
  }

  $form['#id'] = 'versioncontrol-account-form';

  $form['#repository'] = $repository;
  $form['#vcs'] = $repository['vcs'];
  $form['#vcs_name'] = $vcs_name;
  $form['#uid'] = $uid;
  $form['#original_username'] = $vcs_username;

  $form['#validate'] = array('versioncontrol_account_edit_form_validate');
  $form['#submit'] = array('versioncontrol_account_edit_form_submit');

  if ($create_account) {
    $registration_message = versioncontrol_get_repository_registration_message($repository['repo_id']);

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

  $admin_access = versioncontrol_admin_access();

  if ($create_account || $admin_access) {
    if ($create_account) {
      // Have a nice default value for the new VCS username.
      $vcs_username = versioncontrol_account_username_suggestion($repository, $user);
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
      '#default_value' => $vcs_username,
      '#weight' => 0,
      '#size' => 30,
      '#maxlength' => 64,
    );
  }
  else {
    $form['account_name'] = array(
      '#type' => 'value',
      '#value' => $vcs_username,
    );
    $form['account']['account_name_display'] = array(
      '#type' => 'item',
      '#title' => t('@vcs username', array('@vcs' => $vcs_name)),
      '#description' => t('Your @vcs username. This field can only be edited by administrators and is used to link your @vcs messages to your user account.', array('@vcs' => $vcs_name)),
      '#value' => $vcs_username,
      '#weight' => 0,
    );
  }

  if ($uid !== VERSIONCONTROL_REGISTER_DEMO) {
    $form['submit'] = array(
      '#type' => 'submit',
      '#value' => $create_account
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
function versioncontrol_account_edit_form_validate($form, &$form_state) {
  if (!isset($form_state['values']['account_name'])) {
    return;
  }
  $uid = $form['#uid'];
  $username = trim($form_state['values']['account_name']);
  $repository = $form['#repository'];
  $vcs_name = $form['#vcs_name'];

  if (!isset($repository)) { // admin deletes repo while user fills in form
    form_set_error('account',
      t('The repository has been deleted.')
    );
    return;
  }

  if (empty($username)) {
    form_set_error('account_name',
      t('The @vcs username may not be empty.', array('@vcs' => $vcs_name))
    );
  }
  else {
    // Check for username validity - done by the backend, but with a fallback
    // for alphanum-only characters.
    if (!versioncontrol_is_account_username_valid($repository, $username)) {
      form_set_error('account_name',
        t('The specified @vcs username is invalid.', array('@vcs' => $vcs_name))
      );
    }
    // The username is passed by reference and might have changed. That's a bit
    // uncomfortable as a caller API, but more convenient for the backends.
    // (Plus it makes sense anyways since we have trimmed the username too.)
    $form_state['values']['account_name'] = $username;

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
function versioncontrol_account_edit_form_submit($form, &$form_state) {
  // Reconstruct the user data from the $form_state that was passed.
  $uid = $form['#uid'];
  $username = $form_state['values']['account_name'];
  $repository = $form['#repository'];
  $vcs_name = $form['#vcs_name'];
  $vcs_specific = NULL;

  // Let other modules provide additional account data.
  $additional_data = array();
  foreach (module_implements('versioncontrol_account_submit') as $module) {
    $function = $module .'_versioncontrol_account_submit';
    $function($additional_data, $form, $form_state);
  }

  if (empty($form['#original_username'])) {
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
    if (versioncontrol_admin_access()) {
      $message = drupal_set_message(t(
        'The @vcs account %username has been updated successfully.',
        array('@vcs' => $vcs_name, '%username' => $username)
      ));
    }
  }

  $form_state['redirect'] = versioncontrol_admin_access()
    ? 'admin/project/versioncontrol-accounts'
    : 'user/'. $uid .'/edit/versioncontrol/'. $repository['repo_id']
        .'/'. drupal_urlencode($username);
}
