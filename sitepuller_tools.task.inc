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

  // startup message
  $machine = $remote['host'];
  $site = $remote['site_name'];
  $local_site = $local['site_name'];
  $cmd[] = "echo '*********** sitepuller ***********'";
  $cmd[] = "echo 'Moving site $site, subsite $remote_subsite'";
  $cmd[] = "echo 'From $machine to your local machine'";
  $cmd[] = "echo 'Local site $local_site, subsite $local_subsite'";
  $cmd[] = "echo '*********** sitepuller ***********'";

  // delete old stash
  if ($form_state['values']['fresh_copy']) {
    $stash = $remote['stash'];
    $cmd[] = _sitepuller_tools_format_remote_cmd($remote, "rm --force --recursive $stash");
  }

  // backup database on remote machine
  if ($task['pull_database']) {
    $cmd[] = "echo 'backing up database'";
    $db_connect = $task['remote_db_connect'];
    $mysqldump = _sitepuller_tools_mysqldump($db_connect);
    $target_dir = implode('/', array($remote['stash_dir'], 'db'));
    $cmd[] = _sitepuller_tools_format_remote_cmd($remote, _sitepuller_tools_make_dir($target_dir));
    $target = "$target_dir/$remote_subsite.sql";
    $cmd[] = _sitepuller_tools_format_remote_cmd($remote, "$mysqldump > $target");

    // pull database to local machine
    $cmd[] = "echo 'pulling database to local machine'";
    $local_target_dir = implode('/', array($local['stash_dir'], 'db'));
    $cmd[] = _sitepuller_tools_make_dir($local_target_dir);
    $local_target = "$local_target_dir/$remote_subsite.sql";
    $user = $remote['user'];
    $host = $remote['host'];
    $cmd[] = "rsync -acz -e ssh $user@$host:$target $local_target";
    $cmd[] = "echo 'Stored database here: $local_target'";
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
  _sitepuller_tools_write_script($target, $cmd);
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

function _sitepuller_tools_write_script($temp_dir, $cmds) {
  $text = implode(" || exit 1\n", $cmds);
  if (!is_dir($temp_dir)) {
    mkdir($temp_dir, 0775, true);
  }
  $file = "$temp_dir/sitepuller.sh";
  file_put_contents($file, $text);
  chmod($file, 0775);
  dsm($file);
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
 * this scp fails
 * An error occurred: Error on cmd: 'scp jgr25@victoria02.library.cornell.edu:/tmp/sitepuller/music.test2.library.cornell.edu/stash.tar.gz /Users/jgr25/Sites/backups/sitepuller/music/stash.tar.gz' ; exit '1'
 * @param  [type] $remote [description]
 * @param  [type] $local  [description]
 * @return [type]         [description]
 */
function _sitepuller_tools_local_scp_copy($remote, $local) {
  $remote_archive = $remote['tarball'];
  $local_archive = $local['tarball'];
  $remote_user = $remote['user'];
  $remote_machine = $remote['host'];
  $cmd = array(
   "scp $remote_user@$remote_machine:$remote_archive $local_archive"
   );
  $out = _sitepuller_tools_local_cmds($cmd);
  dsm($out);
}
/**
 * this rsync fails
 * An error occurred: Error on cmd: 'rsync -acz -e ssh jgr25@victoria02.library.cornell.edu:/tmp/sitepuller/music.test2.library.cornell.edu/stash/ /Users/jgr25/Sites/backups/sitepuller/music/stash/' ; exit '255'
 * @param  [type] $remote [description]
 * @param  [type] $local  [description]
 * @return [type]         [description]
 */
function _sitepuller_tools_local_rsync_copy($remote, $local) {
  $remote_archive = $remote['stash'];
  $local_archive = $local['stash'];
  $remote_user = $remote['user'];
  $remote_machine = $remote['host'];
  $cmd = array(
   "rsync -acz -e ssh $remote_user@$remote_machine:$remote_archive/ $local_archive/"
   );
  $out = _sitepuller_tools_local_cmds($cmd);
  dsm($out);
}

/**
 * this rsync fails
 * An error occurred: Error on step: 'rsync -acz -e ssh jgr25@victoria02.library.cornell.edu:/tmp/sitepuller/music.test2.library.cornell.edu/stash/ /Users/jgr25/Sites/backups/sitepuller/music/stash/' ; exit '255' ; error 'Permission denied, please try again. Received disconnect from 132.236.27.167: 2: Too many authentication failures for jgr25 rsync: connection unexpectedly closed (0 bytes received so far) [receiver] rsync error: unexplained error (code 255) at io.c(600) [receiver=3.0.6] '
 * @param  [type] $ssh    [description]
 * @param  [type] $remote [description]
 * @param  [type] $local  [description]
 * @return [type]         [description]
 */
function _sitepuller_tools_remote_rsync_copy_old($ssh, $remote, $local) {
  $remote_archive = $remote['stash'];
  $local_archive = $local['stash'];
  $remote_user = $remote['user'];
  $remote_machine = $remote['host'];
  $cmd = array(
    "rsync -acz -e ssh $remote_user@$remote_machine:$remote_archive/ $local_archive/"
    );
  $out = _sitepuller_tools_ssh_cmds($ssh, $cmd);
  dsm($out);
}

/**
 * this rsync fails
 * An error occurred: Error on step: 'rsync -acz -e ssh /tmp/sitepuller/music.test2.library.cornell.edu/stash/ jgr25@Jamess-MacBook-Air.local:/Users/jgr25/Sites/backups/sitepuller/music/stash/' ; exit '12' ; error 'ssh: Jamess-MacBook-Air.local: Name or service not known rsync: connection unexpectedly closed (0 bytes received so far) [sender] rsync error: error in rsync protocol data stream (code 12) at io.c(600) [sender=3.0.6] '
 * @param  [type] $ssh    [description]
 * @param  [type] $remote [description]
 * @param  [type] $local  [description]
 * @return [type]         [description]
 */
function _sitepuller_tools_remote_rsync_copy($ssh, $remote, $local) {
  $remote_archive = $remote['stash'];
  $local_archive = $local['stash'];
  $remote_user = $remote['user'];
  $remote_machine = $remote['host'];
  $cmd = array(
    "rsync -acz -e 'ssh -l $remote_user' $remote_archive/ jgr25@Jamess-MacBook-Air.local:$local_archive/"
    );
  $out = _sitepuller_tools_ssh_cmds($ssh, $cmd);
  dsm($out);
}

function _sitepuller_tools_remote_rsync_hail_mary($remote, $local) {
  $remote_archive = $remote['stash'];
  $local_archive = $local['stash'];
  $remote_user = $remote['user'];
  $remote_machine = $remote['host'];
  $cmd = array(
    "rsync -acz -e 'ssh -l $remote_user' $remote_user@$remote_machine:$remote_archive/ $local_archive/"
    );
  $out = _sitepuller_tools_local_cmds($cmd);
  dsm($out);
}

