<?php
/**
 * @file
 * Code for the Sitepuller-Tools module Task batch completion.
 */

function sitepuller_tools_perform_task($task_nid, $remote_subsite, $local_subsite, $form_state, &$context) {
  try {
    $major_steps = array(
      "delete old archive",
      "archive core",
      "archive sites/all",
      "archive sites/default",
      "archive subsites",
      "compress archive",
      "move archive to local",
      "decompress archive",
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
      case "delete old archive":
        $remote = _sitepuller_tools_info_remote($task['remote_site_nid']);
        $ssh = _sitepuller_tools_get_ssh($remote['host'], $remote['user']);
        _sitepuller_tools_delete_remote_path($ssh, $remote['tarball']);
        $context['results'][] = t('Deleted tarball');
        if ($form_state['values']['fresh_copy']) {
          _sitepuller_tools_delete_remote_path($ssh, $remote['stash']);
          $context['results'][] = t('Deleted old archive');
        }
      break;

      case 'archive core':
        if ($task['pull_drupal']) {
          $remote = _sitepuller_tools_info_remote($task['remote_site_nid']);
          $ssh = _sitepuller_tools_get_ssh($remote['host'], $remote['user']);
          _sitepuller_tools_pull_drupal_core($ssh, $remote['stash'], $remote['root']);
          $context['results'][] = t('Archived Drupal core');
        }
      break;

      case 'archive sites/all':
        if ($task['pull_code']) {
          $remote = _sitepuller_tools_info_remote($task['remote_site_nid']);
          $ssh = _sitepuller_tools_get_ssh($remote['host'], $remote['user']);
          _sitepuller_tools_pull_code($ssh, $remote['stash'], $remote['root']);
          $context['results'][] = t('Archived Drupal core');
        }
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
    $error = $results['error'];
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
