<?php
// $Id$
/**
 * @file
 * Common code for VCS hook scripts that interface with Version Control API.
 *
 * Copyright 2006, 2007 by Derek Wright ("dww", http://drupal.org/user/46549)
 * Copyright 2007 by Adam Light ("aclight", http://drupal.org/user/86358)
 * Copyright 2007 by Jakob Petsovits (http://drupal.org/user/56020)
 *
 * Distributed under the GNU General Public Licence version 2,
 * as published by the FSF on http://www.gnu.org/copyleft/gpl.html
 */

/**
 * Bootstrap the Drupal installation in $drupal_path with the given $phase.
 * As opposed to drupal_bootstrap(), this one doesn't set the
 * Content-Type header when called with DRUPAL_BOOTSTRAP_FULL.
 * It may be called from whatever working directory, and will exit(1)
 * in case an error occurs.
 */
function versioncontrol_bootstrap($drupal_path, $phase) {
  // add $drupal_path to current value of the PHP include_path
  set_include_path(get_include_path() . PATH_SEPARATOR . $drupal_path);

  $current_directory = getcwd();
  chdir($drupal_path);

  // bootstrap Drupal so we can use drupal functions to access the databases, etc.
  if (!file_exists('./includes/bootstrap.inc')) {
    $stderr = fopen("php://stderr", "w");
    fwrite($stderr, "Error: failed to load Drupal's bootstrap.inc file.\n");
    exit(1);
  }
  require_once './includes/bootstrap.inc';

  // Do DRUPAL_BOOTSTRAP_FULL manually, because we don't like the HTTP header
  // that is set by _drupal_bootstrap_full().
  if ($phase == DRUPAL_BOOTSTRAP_FULL) {
    drupal_bootstrap(DRUPAL_BOOTSTRAP_PATH); // up to the last usable phase
    require_once './includes/common.inc'; // as in _drupal_bootstrap()
    _versioncontrol_bootstrap_full(); // instead of _drupal_bootstrap_full()
  }
  else {
    drupal_bootstrap($phase);
  }

  chdir($current_directory);
}

/**
 * A customized version of _drupal_bootstrap_full() in common.inc that doesn't
 * set the Content-Type header. Apart from that, it's a plain copy.
 */
function _versioncontrol_bootstrap_full() {
  static $called;
  global $locale;

  if ($called) {
    return;
  }
  $called = 1;
  require_once './includes/theme.inc';
  require_once './includes/pager.inc';
  require_once './includes/menu.inc';
  require_once './includes/tablesort.inc';
  require_once './includes/file.inc';
  require_once './includes/unicode.inc';
  require_once './includes/image.inc';
  require_once './includes/form.inc';
  // Set the Drupal custom error handler.
  set_error_handler('error_handler');
  // Emit the correct charset HTTP header.
  //drupal_set_header('Content-Type: text/html; charset=utf-8');
  // Detect string handling method
  unicode_check();
  // Undo magic quotes
  fix_gpc_magic();
  // Load all enabled modules
  module_load_all();
  // Initialize the localization system.  Depends on i18n.module being loaded already.
  $locale = locale_initialize();
  // Let all modules take action before menu system handles the reqest
  module_invoke_all('init');
}
