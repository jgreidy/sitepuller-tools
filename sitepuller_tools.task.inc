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

    $step = $context['sandbox']['step'];
    switch ($major_steps["$major_step"]) {
      case "delete old archive":
        $context['message'] = t('Deleting old archive.');
        $task = _sitepuller_tools_info_task($task_nid);
        $remote = _sitepuller_tools_info_remote($task['remote_site_nid']);
        $ssh = _sitepuller_tools_get_ssh($remote['host'], $remote['user']);
        _sitepuller_tools_delete_remote_path($ssh, $remote['tarball']);
        $context['results'][] = t('Deleted tarball');
        if ($form_state['values']['fresh_copy']) {
          _sitepuller_tools_delete_remote_path($ssh, $remote['stash']);
          $context['results'][] = t('Deleted old archive');
        }
        break;

      default:
        throw new Exception(t('Unknown task step: @step', array('@step' => $major_steps["$major_step"])), 1);
        break;
    }
    $context['sandbox']['step']++;

    // Inform the batch engine that we are not finished,
    // and provide an estimation of the completion level we reached.
    $context['finished'] = ($context['sandbox']['major_step'] + ($context['sandbox']['step'] / $context['sandbox']['step_count'])) / $context['sandbox']['major_step_count'];

    if ($context['sandbox']['step'] >= $context['sandbox']['step_count']) {
      $context['sandbox']['major_step']++;
      $context['sandbox']['step'] = 0;
      $context['sandbox']['step_count'] = 1;
    }
  }
  catch (Exception $e) {
    $context['finished'] = 1.0;
    $context['message'] = $e->getMessage();
    $current_set = &_batch_current_set();
    $current_set['success'] = FALSE;
    throw $e;
  }
}

function sitepuller_tools_perform_task_final($success, $results, $operations) {
  if ($success) {
    // Here we do something meaningful with the results.
    $message = t('@count items successfully processed:', array('@count' => count($results)));
    // $message .= theme('item_list', $results);  // D6 syntax
    $message .= theme('item_list', array('items' => $results));
    drupal_set_message($message);
  }
  else {
    // An error occurred.
    // $operations contains the operations that remained unprocessed.
    $error_operation = reset($operations);
    $message = t('An error occurred while processing %error_operation with arguments: @arguments', array('%error_operation' => $error_operation[0], '@arguments' => print_r($error_operation[1], TRUE)));
    drupal_set_message($message, 'error');
  }
 }

function _sitepuller_tools_delete_remote_path($ssh, $remote_path) {
  $cmd = array(
    'rm --force --recursive "' . $remote_path . '"',
    );
  _sitepuller_tools_ssh_cmds($ssh, $cmd);
}
