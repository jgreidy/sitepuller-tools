<?php
/**
 * @file
 * Code for the Sitepuller-Tools module Task batch completion.
 */

/**
 * batch processor for moving pulling Drupal site to local machine
 * @param  integer $task_nid       node id od sitepuller task
 * @param  string $remote_subsite name of selected subsite on remote machine
 * @param  string $local_subsite  name of corresponding subsite on local machine
 * @param  array $form_state     from task form
 * @param  array &$context       batch api info
 * @return exception                 throws exception to stop batch process on error
 */
function sitepuller_tools_perform_task($task_nid, $remote_subsite, $local_subsite, $form_state, &$context) {
  try {
    $major_steps = array(
      "delete old stash",
      "stash core",
      "stash sites/all",
      "stash sites/default",
      "stash subsites",
      "compress stash",
      "move stash to local",
      "decompress stash",
      "install core",
      "install sites/all",
      "install sites/default",
      "install subsites",
      );

    if (!isset($context['sandbox']['major_step'])) {
      $context['sandbox']['major_step'] = 0;
      $context['sandbox']['major_step_count'] = count($major_steps);
      $context['sandbox']['step'] = 0;
      $context['sandbox']['step_count'] = 1;
    }

    if(!function_exists('_sitepuller_tools_get_ssh')) {
      module_load_include('rules.inc', 'sitepuller_tools', $name = NULL);
    }

    $major_step = $context['sandbox']['major_step'];
    if ($major_step >= count($major_steps)) {
      throw new Exception("Invalid task step number", 1);
    }

    $task = _sitepuller_tools_info_task($task_nid);
    //dsm($task);

    $step = $context['sandbox']['step'];
    switch ($major_steps["$major_step"]) {
      case "delete old stash":
        $remote = _sitepuller_tools_info_remote($task['remote_site_nid']);
        $ssh = _sitepuller_tools_get_ssh($remote['host'], $remote['user']);
        _sitepuller_tools_delete_remote_path($ssh, $remote['tarball']);
        $context['results'][] = t('Deleted old tarball');
        if ($form_state['values']['fresh_copy']) {
          _sitepuller_tools_delete_remote_path($ssh, $remote['stash']);
          $context['results'][] = t('Deleted old stash');
        }
      break;

      case 'stash core':
        if ($task['pull_drupal']) {
          $remote = _sitepuller_tools_info_remote($task['remote_site_nid']);
          $ssh = _sitepuller_tools_get_ssh($remote['host'], $remote['user']);
          _sitepuller_tools_pull_drupal_core($ssh, $remote['stash'], $remote['root']);
          $context['results'][] = t('Stashed Drupal core');
        }
      break;

      case 'stash sites/all':
        if ($task['pull_code']) {
          $remote = _sitepuller_tools_info_remote($task['remote_site_nid']);
          $ssh = _sitepuller_tools_get_ssh($remote['host'], $remote['user']);
          _sitepuller_tools_pull_code($ssh, $remote['stash'], $remote['root']);
          $context['results'][] = t('Stashed sites/all');
        }
      break;

      case "stash sites/default":
          $remote = _sitepuller_tools_info_remote($task['remote_site_nid']);
          $ssh = _sitepuller_tools_get_ssh($remote['host'], $remote['user']);
          _sitepuller_tools_pull_subsite($ssh, $remote['stash'], $remote['root'],
            'default', $task);
          $context['results'][] = t('Stashed sites/default');
      break;

      case "stash subsites":
        if ($remote_subsite != 'default') {
          $remote = _sitepuller_tools_info_remote($task['remote_site_nid']);
          $ssh = _sitepuller_tools_get_ssh($remote['host'], $remote['user']);
          _sitepuller_tools_pull_subsite($ssh, $remote['stash'], $remote['root'],
            $remote_subsite, $task);
          $context['results'][] = t('Stashed sites/@ss', array('@ss' => $remote_subsite));
        }
      break;

      case "compress stash":
        $remote = _sitepuller_tools_info_remote($task['remote_site_nid']);
        $ssh = _sitepuller_tools_get_ssh($remote['host'], $remote['user']);
        $cmd = array(
          "tar -czf " . $remote['tarball'] . " " . $remote['stash'],
          );
         _sitepuller_tools_ssh_cmds($ssh, $cmd, t('Failed to compress @stash into @tarball',
          array('@stash' => $remote['stash'], '@tarball' => $remote['tarball'])));
          $context['results'][] = t('Compressed stash into @tb', array('@tb' => $remote['tarball']));
      break;

      case "move stash to local":
        $local = _sitepuller_tools_info_local($task['local_site_nid']);
        $remote = _sitepuller_tools_info_remote($task['remote_site_nid']);
        _sitepuller_tools_move_tarball($remote, $local, &$context);
      break;

      default:
        $context['results'][] = t('unknown step: @step', array ('@step' => $major_steps["$major_step"]));
        //throw new Exception(t('Unknown task step: @step', array('@step' => $major_steps["$major_step"])), 1);
      break;
    }
    $context['sandbox']['step']++;

    // Inform the batch engine that we are not finished,
    // and provide an estimation of the completion level we reached.
    $context['finished'] = ($context['sandbox']['major_step'] + ($context['sandbox']['step'] / $context['sandbox']['step_count'])) / $context['sandbox']['major_step_count'];

    if ($context['sandbox']['step'] >= $context['sandbox']['step_count']) {
      $next = ++$context['sandbox']['major_step'];
      $context['sandbox']['step'] = 0;
      $context['sandbox']['step_count'] = 1;
      $context['message'] = empty($major_steps["$next"]) ? 'done' : $major_steps["$next"];
    }
  }
  catch (Exception $e) {
    $context['finished'] = 1.0;
    $context['results']['error'] = $e->getMessage();
  }
}

function sitepuller_tools_perform_task_final($success, $results, $operations) {
  if ($success && empty($results['error'])) {
    // Here we do something meaningful with the results.
    $message = t('@count steps successfully taken:', array('@count' => count($results)));
    // $message .= theme('item_list', $results);  // D6 syntax
    $message .= theme('item_list', array('items' => $results));
    drupal_set_message($message);
  }
  else {
    // An error occurred.
    $error = empty($results['error']) ? t('error') : $results['error'];
    unset ($results['error']);
    if (count($results) > 0) {
      $message = t('Finished these steps:');
      $message .= theme('item_list', array('items' => $results));
      drupal_set_message($message);
    }
    $message = t('An error occurred: %error_operation', array('%error_operation' => $error));
    drupal_set_message($message, 'error');
  }
 }

_sitepuller_tools_move_tarball($remote, $local, $context) {
  $step = $context['sandbox']['step'];
  $sftp = _sitepuller_tools_get_sftp($remote['host'], $remote['user']);
  if ($step == 0) {
    $filesize = $sftp->size($remote['tarball']);
    $blocksize = 1024 * 512;
    $full_blocks = floor($filesize / $blocksize);
    $last_block_size = $filesize - ($full_blocks * $blocksize);
    $download = array(
      'filesize' => $filesize,
      'blocksize' => $blocksize,
      'full_blocks' => $full_blocks,
      'last_block_size' => $last_block_size,
      'current_block' => 0,
      );
    $context['sandbox']['step_count'] = $full_blocks + 1 + 1;
    $context['sandbox']['download'] = $download;
  }
  else {
    $start = $context['sandbox']['download']['current_block']++ * $context['sandbox']['download']['blocksize'];
    if ($context['sandbox']['download']['current_block'] < $context['sandbox']['download']['full_blocks']) {
      $len = $context['sandbox']['download']['blocksize'];
    }
    else {
      // last block may be shorter
      $len = $context['sandbox']['download']['last_block_size'];
      $context['results'][] = t('Moved tarball to @tb', array('@tb' => $local['tarball']));
    }
    $block = $sftp->get($remote['tarball'], FALSE, $start, $len);
    if ($block === FALSE) {
      throw new Exception(t("Failed to pull @rt from @rh",
        array('@rt' => $remote['tarball'], '@rh' => $remote['host'])), 1);
    }
  }
  $context['message'] = t('Copied @count of @n blocks from @rh.', array(
    '@count' => $context['sandbox']['download']['current_block'],
    '@n' => $context['sandbox']['download']['full_blocks'] + 1,
    '@rh' => $remote['host'],
    ));

}

function _sitepuller_tools_delete_remote_path($ssh, $remote_path) {
  $cmd = array(
    'rm --force --recursive "' . $remote_path . '"',
    );
  _sitepuller_tools_ssh_cmds($ssh, $cmd);
}

function _sitepuller_tools_pull_drupal_core($ssh, $remote_path, $site_root) {
  $target = "$remote_path/core";
  $cmd = array(
    "mkdir -p $target",
    "test -d $target",
    "rsync -acz --exclude=sites --delete $site_root/ $target/",
    );
  $out = _sitepuller_tools_ssh_cmds($ssh, $cmd);
}

function _sitepuller_tools_pull_code($ssh, $remote_path, $site_root) {
  $target = "$remote_path/sites/all";
  $cmd = array(
    "mkdir -p $target",
    "test -d $target",
    "rsync -acz --exclude=sites --delete $site_root/sites/all/ $target/",
    );
  $out = _sitepuller_tools_ssh_cmds($ssh, $cmd);
}

function _sitepuller_tools_pull_subsite($ssh, $remote_path, $site_root, $subsite, $task) {
  $target = "$remote_path/sites/$subsite";
  $subsite_path = "$site_root/sites/$subsite";

  if ($task['pull_files']) {
    $public = _sitepuller_tools_find_public_files_path($ssh, $site_root, $subsite_path)
    $target_path = "$target/files";
    $cmd = array(
      "mkdir -p $target_path",
      "test -d $target_path",
      "rsync -avcz --delete $public/ $target_path/"
      );
    $out = _sitepuller_tools_ssh_cmds($ssh, $cmd, t("Can't copy public files for subsite @p", array('@p' => $subsite_path));
  }
  if ($task['pull_pivate']) {
    $private = _sitepuller_tools_find_private_files_path($ssh, $site_root, $subsite_path)
    $target_path = "$target/private/files";
    $cmd = array(
      "mkdir -p $target_path",
      "test -d $target_path",
      "rsync -avcz --delete $private/ $target_path/"
      );
    $out = _sitepuller_tools_ssh_cmds($ssh, $cmd, t("Can't copy private files for subsite @p", array('@p' => $subsite_path));
  }
  if ($task['pull_database']) {
    $target_path = "$target/database";
    $cmd = array(
      "mkdir -p $target_path",
      "test -d $target_path",
      "cd $site ; drush sql-dump --ordered-dump > $target_path/database.sql"
      );
    $out = _sitepuller_tools_ssh_cmds($ssh, $cmd);
  }
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
  $cmd = "php -r 'echo realpath(\"$path\");'"
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
  $cmd = "php -r 'echo realpath(\"$path\");'"
  $realpath = $ssh->exec($cmd);
  return $realpath;
}
