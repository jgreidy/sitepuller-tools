<?php
/**
 * @file
 * Code for the Sitepuller-Tools module Task batch completion.
 */

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

  // delete old stash
  if ($form_state['values']['fresh_copy']) {
    $stash = $remote['stash'];
    $cmd[] = _sitepuller_tools_format_remote_cmd($remote, "rm --force --recursive $stash");
  }

  $rsync_stash_to_local = FALSE;
  $copy_files_to_stash = empty($form_state['values']['copy_files']) ? FALSE : TRUE;

  // backup database on remote machine
  if ($task['pull_database']) {
    $rsync_stash_to_local = TRUE;
    $cmd[] = "echo 'backing up database'";
    $db_connect = $task['remote_db_connect'];
    $mysqldump = _sitepuller_tools_mysqldump($db_connect);
    $target_dir = implode('/', array($remote['stash_dir'], 'db'));
    $cmd[] = _sitepuller_tools_format_remote_cmd($remote, _sitepuller_tools_make_dir($target_dir));
    $target = "$target_dir/$remote_subsite.sql";
    $cmd[] = _sitepuller_tools_format_remote_cmd($remote, "$mysqldump > $target");
  }

  if ($copy_files_to_stash) {
    $rsync_stash_to_local = TRUE;

    // copy drupal core
    if ($task['pull_drupal']) {
      $cmd[] = "echo 'copying drupal core'";
      $source = $remote['root'];
      $target = implode('/', array($remote['stash_dir'], 'drupal'));
      $user = $remote['user'];
      $machine = $remote['host'];
      $cmd[] = _sitepuller_tools_format_remote_cmd($remote, _sitepuller_tools_make_dir($target));
      $cmd[] = _sitepuller_tools_format_remote_cmd($remote, "rsync -e ssh -acz --omit-dir-times --exclude=sites $source/ $target/");
    }

    // copy sites/all
    if ($task['pull_code']) {
      $cmd[] = "echo 'copying modules, themes, libraries'";
      $source = $remote['root'] . "/sites/all";
      $target = implode('/', array($remote['stash_dir'], 'drupal', 'sites' , 'all'));
      $cmd[] = _sitepuller_tools_format_remote_cmd($remote, _sitepuller_tools_make_dir($target));
      $cmd[] = _sitepuller_tools_format_remote_cmd($remote, "rsync -e ssh -acz --exclude=drush --delete $source/ $target/");
    }

    // copy subsite
    $cmd[] = "echo 'copying subsite'";
    $source = $remote['root'] . "/sites/$remote_subsite";
    $target = implode('/', array($remote['stash_dir'], 'drupal', 'sites' , $local_subsite));
    $cmd[] = _sitepuller_tools_format_remote_cmd($remote, _sitepuller_tools_make_dir($target));
    $cmd[] = _sitepuller_tools_format_remote_cmd($remote, "sudo cd $target", TRUE);
    $cmd[] = _sitepuller_tools_format_remote_cmd($remote, "sudo rsync -e ssh -acz --super --exclude=files --exclude=private --exclude=settings.php --delete . $target/", TRUE);

    // copy public files
    if ($task['pull_files']) {
      $cmd[] = "echo 'copying public files'";
      $public_path = $task['public_files_path'];
      $source = $remote['root'] . "/sites/$remote_subsite/$public_path";
      $target = implode('/', array($remote['stash_dir'], 'drupal', 'sites' , $local_subsite, 'files'));
      $cmd[] = _sitepuller_tools_format_remote_cmd($remote, _sitepuller_tools_make_dir($target));
      $cmd[] = _sitepuller_tools_format_remote_cmd($remote, "sudo rsync -e ssh -acz --delete $source/ $target/");
    }

    // copy private files
    if ($task['pull_private']) {
      $cmd[] = "echo 'copying private files'";
      $public_path = $task['private_files_path'];
      $source = $remote['root'] . "/sites/$remote_subsite/$public_path";
      $target = implode('/', array($remote['stash_dir'], 'drupal', 'sites' , $local_subsite, 'private', 'files'));
      $cmd[] = _sitepuller_tools_format_remote_cmd($remote, _sitepuller_tools_make_dir($target));
      $cmd[] = _sitepuller_tools_format_remote_cmd($remote, "sudo rsync -e ssh -acz --delete $source/ $target/", TRUE);
    }
  }

  if ($rsync_stash_to_local) {
    // pull stash to local machine
    $cmd[] = "echo 'pulling stashed files to local machine'";
    $source = $remote['stash_dir'];
    $target = $local['stash_dir'];
    $cmd[] = _sitepuller_tools_make_dir($target);
    $user = $remote['user'];
    $host = $remote['host'];
    $cmd[] = "rsync -acz -e ssh $user@$host:$source/ $target/";
    $cmd[] = "echo 'Stored files here: $target'";
  }

  if (!$copy_files_to_stash) {

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
    $cmd[] = "rsync -acz -e ssh --exclude=files --exclude=private --exclude=settings.php $user@$host:$source/ $target/";
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
  }

  // install database
  if ($task['pull_database']) {
    $cmd[] = "echo 'restoring database'";
    $subsite_root = $local['site_root'] . "/sites/$local_subsite";
    $local_target_dir = implode('/', array($local['stash_dir'], 'db'));
    $db_backup = "$local_target_dir/$remote_subsite.sql";
    $db_connect = $task['local_db_connect'];
    $cmd[] = "cd $subsite_root";
    $cmd[] = "drush sql-drop --yes";
    $cmd[] = "$db_connect < $db_backup";

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
  }

  $target = $local['stash_dir'] . "/scripts";
  $script_path = _sitepuller_tools_write_script($task_nid, $target, $cmd);

  drupal_set_message(t('The sitepuller script for this task is here:<p>@script</p>', array('@script' => $script_path)), 'status', FALSE);
}

function _sitepuller_tools_make_dir($path) {
  return "test -d $path || mkdir -p $path";
}

function _sitepuller_tools_format_remote_cmd($remote, $cmd, $use_t = FALSE) {
  $user = $remote['user'];
  $host = $remote['host'];
  $flags = $use_t ? '-t' : '';
  $remote_cmd = "ssh $flags $user@$host '" . addslashes($cmd) . "'";
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

function _sitepuller_tools_write_script($task_nid, $temp_dir, $cmds) {
  $header = sitepuller_tools_script_header($task_nid);
  $text = implode("\n", $header);
  $step = 100;
  foreach ($cmds as $value) {
    $text .= "\n\n$value || error_exit ss" . $step++;
  }
  if (!is_dir($temp_dir)) {
    mkdir($temp_dir, 0775, true);
  }
  $file = "$temp_dir/sitepuller.sh";
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


