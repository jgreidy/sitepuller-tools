<?php
/**
 * @file
 * Code for the Sitepuller-Tools module Task batch completion.
 */

function _sitepuller_tools_script_init($local, $remote) {
  if(!function_exists('_sitepuller_tools_decrypt_string')) {
    module_load_include('rules.inc', 'sitepuller_tools', $name = NULL);
  }
  $remote_db_connect = _sitepuller_tools_decrypt_string($remote['remote_db_connect']);
  $cmd[] = '#!/bin/bash';
  $cmd[] = "REMOTEDUMP='" . _sitepuller_tools_mysqldump($remote_db_connect) . "'";
  $cmd[] = "LOCALDBC='" . _sitepuller_tools_decrypt_string($local['local_db_connect']) . "'";

  $text = implode(PHP_EOL, $cmd) . PHP_EOL;
  $script_dir = $local['script_dir'];
  $script_name = $local['script_name'];
  if (!is_dir($script_dir)) {
    mkdir($script_dir, 0775, true);
  }
  $file = "$script_dir/$script_name.ini";
  file_put_contents($file, $text);
  chmod($file, 0700);
  return $file;
}

function sitepuller_tools_script_task($task_nid, $form_state) {
  $cmd = array();
  $task = _sitepuller_tools_info_task($task_nid);
  $remote = _sitepuller_tools_info_remote($task['remote_site_nid']);
  $local = _sitepuller_tools_info_local($task['local_site_nid']);

  $remote_subsite = $task['remote_subsite'];
  $local_subsite = $task['local_subsite'];
  $machine = $remote['host'];
  $site = $remote['site_name'];
  $local_site = $local['site_name'];

  // generate name for temp directory
  $fodder = serialize(array(time(),$task,$local,$remote));
  $temp_dir = 't.' . hash("sha256", $fodder);

  // make a local directory to write the scripts to
  $script_dir = $local['tmp'] . '/' . $temp_dir;
  if (!is_dir($script_dir)) {
    mkdir($script_dir, 0700, true);
  }

  // pull drupal core
  if ($task['pull_drupal']) {
    $cmd[] = "echo 'pulling drupal core'";
    $source = $remote['root'];
    $target = $local['site_root'];
    $cmd[] = _sitepuller_tools_make_dir($target);
    $user = $remote['user'];
    $host = $remote['host'];
    $cmd[] = "rsync -acz -e ssh --exclude=sites $user@$host:$source/ $target/";
    $cmd[] = "echo 'Stored core here: $target'";
  }

  // pull sites/all
  if ($task['pull_code']) {
    $cmd[] = "echo 'pulling modules, themes, libraries'";
    $source = $remote['root'] . "/sites/all";
    $target = $local['site_root'] . "/sites/all";
    $cmd[] = _sitepuller_tools_make_dir($target);
    $user = $remote['user'];
    $host = $remote['host'];
    $cmd[] = "rsync -acz -e ssh --exclude=drush --delete $user@$host:$source/ $target/";
    $cmd[] = "echo 'Stored modules, themes, libraries here: $target'";
  }

  // pull subsite
  $cmd[] = "echo 'pulling subsite'";
  $source = $remote['root'] . "/sites/$remote_subsite";
  $target = $local['site_root'] . "/sites/$local_subsite";
  $cmd[] = _sitepuller_tools_make_dir($target);
  $user = $remote['user'];
  $host = $remote['host'];
  $cmd[] = "rsync -acz -e ssh --exclude=files --exclude=private --exclude=settings.php --exclude=drushrc.php $user@$host:$source/ $target/";
  $cmd[] = "echo 'Stored subsite here: $target'";

  // pull public files
  if ($task['pull_files']) {
    $cmd[] = "echo 'pulling public files'";
    $public_path = $task['public_files_path'];
    $source = $remote['root'] . "/sites/$remote_subsite/$public_path";
    $target = $local['site_root'] . "/sites/$local_subsite/files";
    $cmd[] = _sitepuller_tools_make_dir($target);
    $user = $remote['user'];
    $host = $remote['host'];
    $cmd[] = "rsync -acz -e ssh $user@$host:$source/ $target/";
    $cmd[] = "echo 'Stored public files here: $target'";
  }

  // pull private files
  if ($task['pull_private']) {
    $cmd[] = "echo 'pulling private files'";
    $private_path = $task['private_files_path'];
    $source = $remote['root'] . "/sites/$remote_subsite/$private_path";
    $target = $local['site_root'] . "/sites/$local_subsite/private/files";
    $cmd[] = _sitepuller_tools_make_dir($target);
    $user = $remote['user'];
    $host = $remote['host'];
    $cmd[] = "rsync -acz -e ssh $user@$host:$source/ $target/";
    $cmd[] = "echo 'Stored private files here: $target'";
  }

  // call then delete the script to move the database
  if ($task['pull_database']) {
    // make a script to move the database from the remote site to the local site
    $db_move_script_path = _sitepuller_tools_db_move($local, $remote, $remote_subsite, $local_subsite, $temp_dir);
    drupal_set_message(t("$db_move_script_path db move script"));
    $cmd[] = "echo 'pulling database'";
    $cmd[] = "`which bash` $db_move_script_path";
    $cmd[] = "rm -f $db_move_script_path";
  }

  // get rid of the script directory once it's done running
  $cmd[] = "rm -rf $script_dir";

  $script_path = _sitepuller_tools_write_script($task_nid, $script_dir, 'sitepuller.sh', $cmd);

  drupal_set_message(t('The sitepuller script for this task is here:<p>@script</p>', array('@script' => $script_path)), 'status', FALSE);
}

function _sitepuller_tools_make_dir($path) {
  return "test -d $path || mkdir -p $path";
}

function _sitepuller_tools_format_remote_cmd($remote, $cmd) {
  $user = $remote['user'];
  $host = $remote['host'];
  $remote_cmd = "ssh $user@$host '" . addslashes($cmd) . "'";
  return $remote_cmd;
}

function _sitepuller_tools_mysqldump($db_connect) {
  // reformat sql-connect to mysqldump
  // sql-connect - mysql --user=drupaluser --password=123xyz --database=music --host=127.0.0.1 --port=33067
  // mysqldump - mysqldump -u root -p[root_password] [database_name] > dumpfilename.sql
  $db_connect .= ' '; // to find a blank after the last field
  $tests = array('user', 'password', 'database', 'host', 'port');
  $fields = array();
  foreach ($tests as $test) {
    $pattern = "/--$test=([^ ]*) /";
    $matches = array();
    $found = preg_match($pattern, $db_connect, $matches);
    if ($found) {
      $fields["$test"] = $matches[1];
    }
  }
  $out = 'mysqldump -u ' . $fields['user'] . ' -p' . $fields['password'] . ' ';
  if (isset($fields['port'])) {
    $out .= '--port=' . $fields['port'] . ' ';
  }
  $out .= '--order-by-primary ';  // to minimize work for rsync later on
  $out .= $fields['database'];
  return $out;
}

function _sitepuller_tools_script_task_subsite($remote_path, $site_root, $subsite, $task, &$cmd) {
  $target = "$remote_path/sites/$subsite";
  $subsite_path = "$site_root/sites/$subsite";

  if ($task['pull_files']) {
    $public = "$subsite_path/" . $task['public_files_path'];
    $target_path = "$target/files";
    $cmd[] = "mkdir -p $target_path";
    $cmd[] = "test -d $target_path";
    $cmd[] = "rsync -acz --delete $public/ $target_path/";
   }
  if ($task['pull_private']) {
    $private = "$subsite_path/" . $task['private_files_path'];
    $target_path = "$target/private/files";
    $cmd[] = "mkdir -p $target_path";
    $cmd[] = "test -d $target_path";
    $cmd[] = "rsync -acz --delete $private/ $target_path/";
  }
  if ($task['pull_database']) {
    $target_path = "$target/database";
    $cmd[] = "mkdir -p $target_path";
    $cmd[] = "test -d $target_path";
    $cmd[] = "cd $subsite_path";
    $cmd[] = "drush sql-dump --ordered-dump > $target_path/database.sql";
  }
}

function sitepuller_tools_script_header($task_nid) {
  $cmd = array();
  $task = _sitepuller_tools_info_task($task_nid);
  $remote = _sitepuller_tools_info_remote($task['remote_site_nid']);
  $local = _sitepuller_tools_info_local($task['local_site_nid']);

  $remote_subsite = $task['remote_subsite'];
  $local_subsite = $task['local_subsite'];

  $cmd[] = '#!/bin/bash';
  $cmd[] = '# sitepuller.sh - move remote Drupal site to local machine';
  $cmd[] = 'function error_exit {';
  $cmd[] = ' echo "Scrip failed at step: $1" 1>&2';
  $cmd[] = ' exit 1';
  $cmd[] = '}';
  $cmd[] = 'if [ -z "$SSH_AUTH_SOCK" ] ; then';
  $cmd[] = ' eval `ssh-agent -s`';
  $cmd[] = ' ssh-add';
  $cmd[] = 'fi';

  // startup message
  $machine = $remote['host'];
  $site = $remote['site_name'];
  $local_site = $local['site_name'];
  $cmd[] = "echo '*********** sitepuller ***********'";
  $cmd[] = "echo 'Moving site $site, subsite $remote_subsite'";
  $cmd[] = "echo 'From $machine to your local machine'";
  $cmd[] = "echo 'Local site $local_site, subsite $local_subsite'";
  $cmd[] = "echo '*********** sitepuller ***********'";
  return $cmd;
}

function _sitepuller_tools_write_script($task_nid, $script_dir, $script_name, $cmds) {
  $header = sitepuller_tools_script_header($task_nid);
  $text = implode("\n", $header);
  $step = 100;
  foreach ($cmds as $value) {
    $text .= "\n\n$value || error_exit ss" . $step++;
  }
  if (!is_dir($script_dir)) {
    mkdir($script_dir, 0775, true);
  }
  $file = "$script_dir/$script_name";
  file_put_contents($file, $text);
  chmod($file, 0775);
  return $file;
}

function _sitepuller_tools_find_public_files_path($ssh, $site_path, $subsite_path) {
  // Drupal 7 version of public files
  $cmd = "cd $subsite_path ; drush vget file_public_path";
  $out7 = $ssh->exec($cmd);
  $match = array();
  preg_match("/file_public_path: \"([^\"]+)\"/", $out7, $match);
  if (empty($match)) {
    // try for Drupal 6 version
    $cmd = "cd $subsite_path ; drush vget file_directory_path";
    $out6 = $ssh->exec($cmd);
    $match = array();
    preg_match("/file_directory_path: \"([^\"]+)\"/", $out6, $match);
    if (empty($match)) {
      throw new Exception(t("Failed to determine public file path for @s in @p", array('@s' => $subsite_path, '@p' => $out6)));
    }
  }
  $path = $site_path . '/' . $match[1];
  $cmd = "php -r 'echo realpath(\"$path\");'";
  $realpath = $ssh->exec($cmd);
  return $realpath;
}

function _sitepuller_tools_find_private_files_path($ssh, $site_path, $subsite_path) {
  // only available for Drupal 7 sites
  $cmd = "cd $subsite_path ; drush vget file_private_path";
  $out7 = $ssh->exec($cmd);
  $match = array();
  preg_match("/file_private_path: \"([^\"]+)\"/", $out7, $match);
  if (empty($match)) {
    throw new Exception(t("Failed to determine private file path for @s", array('@s' => $subsite_path)));
    }
  $path = $site_path . '/' . $match[1];
  $cmd = "php -r 'echo realpath(\"$path\");'";
  $realpath = $ssh->exec($cmd);
  return $realpath;
}

/**
 * write a script to back up remote database
 * @return string path to script
 */
function _sitepuller_tools_db_move($local, $remote, $remote_subsite, $local_subsite, $temp_dir_name) {
  if(!function_exists('_sitepuller_tools_decrypt_string')) {
    module_load_include('rules.inc', 'sitepuller_tools', $name = NULL);
  }
  $remote_db_connect = _sitepuller_tools_decrypt_string($remote['remote_db_connect']);
  $dump_remote = _sitepuller_tools_mysqldump($remote_db_connect);

  $cmd[] = '#!/bin/bash';

  // remote db dump
  $remote_target_dir = implode('/', array($remote['tmp'], $temp_dir_name));
  $cmd[] = _sitepuller_tools_format_remote_cmd($remote, _sitepuller_tools_make_dir($remote_target_dir));
  $remote_db_sql = "$remote_target_dir/db.sql";
  $cmd[] = _sitepuller_tools_format_remote_cmd($remote, "$dump_remote > $remote_db_sql");

  // copy remote db to local
  $cmd[] = "echo 'pulling stashed files to local machine'";
  $local_db_sql = implode('/', array($local['tmp'], $temp_dir_name, 'db.sql'));
  $user = $remote['user'];
  $host = $remote['host'];
  $cmd[] = "rsync -acz -e ssh $user@$host:$remote_db_sql $local_db_sql";
  //$cmd[] = "echo 'Stored files here: $local_db_sql'";

  // get rid of remote copy
  $cmd[] = _sitepuller_tools_format_remote_cmd($remote, "rm -rf $remote_target_dir");

  // replace local db with remote one
  $cmd[] = "echo 'restoring database'";
  $subsite_root = $local['site_root'] . "/sites/$local_subsite";
  $db_connect = _sitepuller_tools_decrypt_string($local['local_db_connect']);
  $cmd[] = "cd $subsite_root";
  $cmd[] = "drush sql-drop --yes";
  $cmd[] = "$db_connect < $local_db_sql";
  $cmd[] = "drush updatedb --cache-clear=1";

  // delete the local db copy
  $cmd[] =  "echo 'deleting local db dump'";
  $cmd[] = "rm -f $local_db_sql";

  // set up file paths
  $subsite_root = $local['site_root'] . "/sites/$local_subsite";
  $public_files = "sites/$local_subsite/files";
  $private_files = "sites/$local_subsite/private/files";
  $cmd[] = "cd $subsite_root";
  // cover Drupal 6 file system
  $cmd[] = "drush vset file_directory_path $public_files";
  $cmd[] = "drush vset file_directory_temp /tmp";
  // cover Drupal 7 file system
  $cmd[] = "drush vset file_private_path $private_files";
  $cmd[] = "drush vset file_public_path $public_files";
  $cmd[] = "drush vset file_temporary_path /tmp";

  $cmd[] = "drush cc all";

  $local_target_dir = implode('/', array($local['tmp'], $temp_dir_name);

  $text = implode(PHP_EOL, $cmd) . PHP_EOL;
  $file = "$local_target_dir/dbdump.sh";
  file_put_contents($file, $text);
  chmod($file, 0700);
  return $file;
}


