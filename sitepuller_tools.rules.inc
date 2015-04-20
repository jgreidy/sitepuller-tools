<?php
set_include_path(get_include_path() . PATH_SEPARATOR . 'phpseclib');

include('Net/SSH2.php');
include('Crypt/Blowfish.php');

/**
 * Implement hook_rules_action_info().
 */
function sitepuller_tools_rules_action_info() {
	$actions['sitepuller_tools_action_check_remote_site'] = array (
		'label' => t('Sitepuller: Check remote site'),
		'group' => t('Sitepuller'),
		'parameter' => array(
			'node' =>  array(
				'type' => 'node',
				'label' => t('The Sitepuller Remote node.'),
				),
			),
			'provides' => array(
				'sitepuller_tools_message' => array(
					'type' => 'text',
					'label' => t('Message'),
					)
				),
		);
	$actions['sitepuller_tools_action_check_local_site'] = array (
		'label' => t('Sitepuller: Check local site'),
		'group' => t('Sitepuller'),
		'parameter' => array(
			'node' =>  array(
				'type' => 'node',
				'label' => t('The Sitepuller Remote node.'),
				),
			),
			'provides' => array(
				'sitepuller_tools_message' => array(
					'type' => 'text',
					'label' => t('Message'),
					)
				),
		);
	$actions['sitepuller_tools_action_save_task'] = array (
			'label' => t('Sitepuller: Save task'),
			'group' => t('Sitepuller'),
			'parameter' => array(
				'node' => array(
					'type' => 'node',
					'label' => t('The Sitepuller Task node.'),
					),
				),
			'provides' => array(
				'sitepuller_tools_message' => array(
					'type' => 'text',
					'label' => t('Message'),
					)
				),
		);

	return $actions;
}

/**
 * open an ssh connection to the machine
 * 	machine - string - name of remote machine
 *	remote_user - string - user to log in as on remote machines
 *
 *  returns Net_SSH2 ssh object or FALSE
 */
function _sitepuller_tools_get_ssh($machine, $remote_user) {
	$rsa_private = variable_get('sitepuller_tools_private_key_file', '');
	$passphrase = variable_get('sitepuller_tools_passphrase', 'the old one'); // encrypted
	$entropy = variable_get('sitepuller_tools_entropy_file', '');

	if (($entropy_file = file_get_contents($entropy)) === FALSE) {
		throw new Exception(t("can't read contents of entropy file"));
	}
	$hash = sha1($entropy_file);
	$cipher = new Crypt_Blowfish();
	$cipher->setKey($hash);
	$packed = pack("H*" , $passphrase);
	$decrypted = $cipher->decrypt($packed);

	if (($keyfile = file_get_contents($rsa_private)) === FALSE) {
		throw new Exception(t("can't read contents of private key file"));
	}

	$key = new Crypt_RSA();
	$key->setPassword($decrypted);
	$key->loadKey($keyfile);

	$ssh = new Net_SSH2("$machine");
	if (!$ssh->login($remote_user, $key)) {
		throw new Exception(t("can't ssh to @mac as @user", array('@mac' => $machine, '@user' => $remote_user)));
	}

	return $ssh;
}

/**
 * open an sftp connection to the machine
 * 	machine - string - name of remote machine
 *	remote_user - string - user to log in as on remote machines
 *
 *  returns Net_SFTP ssh object or FALSE
 */
function _sitepuller_tools_get_sftp($machine, $remote_user) {
	$rsa_private = variable_get('sitepuller_tools_private_key_file', '');
	$passphrase = variable_get('sitepuller_tools_passphrase', 'the old one'); // encrypted
	$entropy = variable_get('sitepuller_tools_entropy_file', '');

	if (($entropy_file = file_get_contents($entropy)) === FALSE) {
		throw new Exception(t("can't read contents of entropy file"));
	}
	$hash = sha1($entropy_file);
	$cipher = new Crypt_Blowfish();
	$cipher->setKey($hash);
	$packed = pack("H*" , $passphrase);
	$decrypted = $cipher->decrypt($packed);

	if (($keyfile = file_get_contents($rsa_private)) === FALSE) {
		throw new Exception(t("can't read contents of private key file"));
	}

	$key = new Crypt_RSA();
	$key->setPassword($decrypted);
	$key->loadKey($keyfile);

	$sftp = new Net_SFTP("$machine");
	if (!$sftp->login($remote_user, $key)) {
		throw new Exception(t("can't sftp to @mac as @user", array('@mac' => $machine, '@user' => $remote_user)));
	}

	return $sftp;
}

function _sitepuller_tools_remote_site_check($node) {
	$wrapper = entity_metadata_wrapper('node', $node);

	$user = $wrapper->field_sitepuller_remote_user->value();
	$mach = $wrapper->field_sitepuller_remote_host->value();
	$path = $wrapper->field_sitepuller_remote_root->value();
	$name = $wrapper->field_sitepuller_remote_site->value();

	// disable ssh checks on remote site for aegir
	if ($ssh_checks = FALSE) {
		$ssh = _sitepuller_tools_get_ssh($mach, $user);
		$cmd[] = "test -d $path";
		$output = _sitepuller_tools_ssh_cmds($ssh, $cmd, t("Directory does not exist: @d", array("@d" => $path)));
		$cmd[] = "cd $path ; drush core-status --pipe | grep bootstrap | grep Successful";
		$output = _sitepuller_tools_ssh_cmds($ssh, $cmd, t("Can't connect to Drupal database for site '@name'", array('@name' => $name)));
		$directory = "test";
		_sitepuller_tools_prepare_remote_temp($ssh, $wrapper, $directory);
		$subsites = _sitepuller_tools_remote_subsites($ssh, $path);
	}
	else {
		$subsites = array('default');
	}
	_sitepuller_tools_cache_remote_site($node->nid, $name, $mach, $subsites);
	return t("Validated remote site '@name'", array('@name' => $name));
}

function sitepuller_tools_action_check_remote_site($node) {
	try {
		_sitepuller_tools_check_user();
		$sitepuller_tools_message[] = _sitepuller_tools_remote_site_check($node);
	}
	catch (Exception $e) {
		$sitepuller_tools_message[] = $e->getMessage();
	}
	return array (
		'sitepuller_tools_message' => implode("\r",$sitepuller_tools_message),
		);
}

function _sitepuller_tools_local_site_check($node) {
	$wrapper = entity_metadata_wrapper('node', $node);

	$name = $wrapper->field_sitepuller_local_site_name->value();
	$tmp = $wrapper->field_sitepuller_local_tmp->value();
	$path = $wrapper->field_sitepuller_local_site_root->value();
	$use_make = $wrapper->field_sitepuller_use_make_file->value();
	$make_file = $wrapper->field_sitepuller_drush_make_file ->value();
	$preamble = variable_get('sitepuller_tools_preamble', '');

	if ($use_make != 0) {
		$cmd[] = "test -d $path || drush make --prepare-install --working-copy $make_file $path";
		$output = _sitepuller_tools_local_cmds($cmd, t('drush make failed: @dir', array('@dir' => $make_file)));
	}

	$cmd[0] = "test -d $path";
	$output = _sitepuller_tools_local_cmds($cmd, t('directory does not exist: @dir', array('@dir' => $path)));
	$cmd[0] = $preamble;
	$cmd[] = "cd $path";
	$cmd[] = "drush status | grep Drupal";
	$output = _sitepuller_tools_local_cmds($cmd); //, t('Can not connect to the database for site \'@name\'', array('@name' => $name)));
	$directory = "test";
	_sitepuller_tools_prepare_local_temp($wrapper, $directory);
	$subsites = _sitepuller_tools_local_subsites($path);
	_sitepuller_tools_cache_local_site($node->nid, $name, $subsites);
	return t("Validated local site '@name'", array('@name' => $name));
}

function sitepuller_tools_action_check_local_site($node) {
	try {
		_sitepuller_tools_check_user();
		$sitepuller_tools_message[] = _sitepuller_tools_local_site_check($node);
	}
	catch (Exception $e) {
		$sitepuller_tools_message[] = $e->getMessage();
	}
	return array (
		'sitepuller_tools_message' => implode("\r",$sitepuller_tools_message),
		);
}

function _sitepuller_tools_prepare_remote_temp($ssh, $remote_site_wrapper, $directory = FALSE) {
	$tmp = $remote_site_wrapper->field_sitepuller_remote_tmp->value();
	$user = $remote_site_wrapper->field_sitepuller_remote_user->value();
	$site = $remote_site_wrapper->field_sitepuller_remote_site->value();
	$path = $directory ? "$tmp/sitepuller/$site/$user/$directory" : "$tmp/sitepuller/$site/$user";
	$cmd[] = "mkdir -p $path";
	$cmd[] = "test -d $path";
	_sitepuller_tools_ssh_cmds($ssh, $cmd, t('Failed to create remote directory: @d', array('@d' => $path)));
	return $path;
}

function _sitepuller_tools_prepare_local_temp($local_site_wrapper, $directory = FALSE) {
	$tmp = $local_site_wrapper->field_sitepuller_local_tmp->value();
	$user = variable_get('sitepuller_tools_user', 'no_user');
	$site = $local_site_wrapper->field_sitepuller_local_site_name->value();
	$path = $directory ? "$tmp/sitepuller/$site/$user/$directory" : "$tmp/sitepuller/$site/$user";
	$cmd[] = "mkdir -p $path";
	$cmd[] = "test -d $path";
	_sitepuller_tools_local_cmds($cmd, t('Failed to create local directory: @d', array('@d' => $path)));
	return $path;
}

function _sitepuller_tools_task_check($node) {
	$sitepuller_tools_message = array();
	_sitepuller_tools_check_user();
	$task_wrapper = entity_metadata_wrapper('node', $node);
	$remote_site_nid = $task_wrapper->field_sitepuller_task_remo_site->getIdentifier();
	$remote_site = node_load($remote_site_nid);
	array_push($sitepuller_tools_message, _sitepuller_tools_remote_site_check($remote_site));
	$local_site_nid = $task_wrapper->field_sitepuller_task_local_site->getIdentifier();
	$local_site = node_load($local_site_nid);
	$local_site_wrapper = entity_metadata_wrapper('node', $local_site);
	array_push($sitepuller_tools_message, _sitepuller_tools_local_site_check($local_site));
	$remote_site_wrapper = entity_metadata_wrapper('node', $remote_site);
	$remote_machine =  $remote_site_wrapper->field_sitepuller_remote_host->value();
	$remote_user = $remote_site_wrapper->field_sitepuller_remote_user->value();
	$ssh = _sitepuller_tools_get_ssh($remote_machine, $remote_user);
	return $sitepuller_tools_message;
}

function sitepuller_tools_action_save_task($node) {
	try {
		_sitepuller_tools_task_check($node);
	}
	catch (Exception $e) {
		$sitepuller_tools_message[] = $e->getMessage();
		return array (
			'sitepuller_tools_message' => implode("\r ",$sitepuller_tools_message),
			);
	}
}

function _sitepuller_tools_run_task($task_nid, $remote_subsite, $local_subsite) {

	try {

		//$remote_archive = _sitepuller_tools_build_remote_archive($task_nid);
		$remote_archive = '/tmp/sitepuller/music.test2.library.cornell.edu/jgr25/archive/music.test2.library.cornell.edu.tar.gz';

		$local_archive = _sitepuller_tools_move_remote_archive($task_nid, $remote_archive);

		// throw new Exception("stop with $remote_archive, $local_archive", 1);


		// $remote_temp = _sitepuller_tools_build_remote_backup($task_nid, $remote_subsite, $local_subsite);

		// _sitepuller_tools_rsync_remote_to_local($task_nid, $remote_temp, $local_subsite);

		// _sitepuller_tools_replace_local_db($task_nid, $local_subsite);
	}
	catch (Exception $e) {
		drupal_set_message(t("_sitepuller_tools_run_task failed."), 'warning', FALSE);
		throw $e;
	}

}

function _sitepuller_tools_build_remote_archive($task_nid) {
	$node = node_load($task_nid);
	$task_wrapper = entity_metadata_wrapper('node', $node);

	$remote_site_nid = $task_wrapper->field_sitepuller_task_remo_site->getIdentifier();
	$remote_site = node_load($remote_site_nid);
	$remote_site_wrapper = entity_metadata_wrapper('node', $remote_site);
	$remote_machine =  $remote_site_wrapper->field_sitepuller_remote_host->value();
	$remote_user = $remote_site_wrapper->field_sitepuller_remote_user->value();
	$remote_root = $remote_site_wrapper->field_sitepuller_remote_root->value();
	$remote_site = $remote_site_wrapper->field_sitepuller_remote_site->value();

	$ssh = _sitepuller_tools_get_ssh($remote_machine, $remote_user);
	$remote_temp = _sitepuller_tools_prepare_remote_temp($ssh, $remote_site_wrapper, "archive");

	$remote_archive = "$remote_temp/$remote_site.tar";
	$cmd = array(
		"cd $remote_root ; drush archive-dump @sites --overwrite --destination=$remote_archive",
		);
	_sitepuller_tools_ssh_cmds($ssh, $cmd);

	$remote_archive .= ".gz"; // gzipped and added to file name by archive-dump

	$cmd = array(
		"test -f $remote_archive"
		);
	$out = _sitepuller_tools_ssh_cmds($ssh, $cmd);

	dsm("made remote: $remote_archive");

	return $remote_archive;
}

//obsolete
function _sitepuller_tools_move_remote_archive($task_nid, $remote_archive, &$context) {
  if (!isset($context['sandbox']['progress'])) {
    $context['sandbox']['progress'] = 0;
    $context['sandbox']['current_node'] = $task_nid;
    $context['sandbox']['max'] = 100;
  }
  dsm($context);
	$node = node_load($task_nid);
	$task_wrapper = entity_metadata_wrapper('node', $node);
	$remote_site_nid = $task_wrapper->field_sitepuller_task_remo_site->getIdentifier();
	$remote_site = node_load($remote_site_nid);
	$remote_site_wrapper = entity_metadata_wrapper('node', $remote_site);
	$remote_machine =  $remote_site_wrapper->field_sitepuller_remote_host->value();
	$remote_user = $remote_site_wrapper->field_sitepuller_remote_user->value();
	$remote_root = $remote_site_wrapper->field_sitepuller_remote_root->value();

	$local_site_nid = $task_wrapper->field_sitepuller_task_local_site->getIdentifier();
	$local_site = node_load($local_site_nid);
	$local_site_wrapper = entity_metadata_wrapper('node', $local_site);
	$local_site_root = $local_site_wrapper->field_sitepuller_local_site_root->value();
	$local_site_name = $local_site_wrapper->field_sitepuller_local_site_name->value();
	$local_temp = _sitepuller_tools_prepare_local_temp($local_site_wrapper, "archive");

	$local_archive = "$local_temp/$local_site_name.tar.gz";

	//$ssh = _sitepuller_tools_get_ssh($remote_machine, $remote_user);
	// $cmd = array(
	// 	"scp $remote_user@$remote_machine:$remote_archive $local_archive"
	// 	);
	// _sitepuller_tools_local_cmds($cmd);

	// $cmd = array(
	// 	"whoami",
	// 	"hostname"
	// 	);
	// $out = _sitepuller_tools_local_cmds($cmd);
	// $local_user = $out[0];
	// $local_machine = $out[1];

//$local_user@$local_machine:
//
	// $ssh = _sitepuller_tools_get_ssh($remote_machine, $remote_user);
	// $cmd = array(
	// 	"rsync -acz -e ssh $remote_user@$remote_machine:$remote_archive $local_temp/",
	// 	);
	// $out = _sitepuller_tools_local_cmds($cmd);
	// dsm($out);

	$sftp = _sitepuller_tools_get_sftp($remote_machine, $remote_user);
	// $success = $sftp->get($remote_archive, $local_archive);
	// if ($success === FALSE) {
	// 	throw new Exception("Failed to move $remote_subsite to $local_archive", 1);
	// }

	if (empty($context['sandbox']['filesize'])) {
		$filesize = $sftp->size($remote_archive);
		dsm("Filesize: $filesize");
		$context['sandbox']['filesize'] = $filesize;
		$context['sandbox']['max'] = $filesize;
	}
	else {
		$filesize = $context['sandbox']['filesize'];
	}

	$start_at = $context['sandbox']['current_node'];
	$block_size = 1024;
	$blocks_per_run = 10;
	$run_max = max($filesize, $start_at + $block_size * $blocks_per_run);

	for ($start = $start_at; $start < $run_max; $start += $block_size) {
		$len = min($block_size, $start - $filesize);
		//$success = $sftp->get($remote_archive, $local_archive, $start, $len);
		$success = TRUE;
		if ($success === FALSE) {
			throw new Exception("Failed to move $remote_subsite to $local_archive", 1);
		}
		$context['sandbox']['progress'] = $start;
		$context['sandbox']['message'] = "$start / $filesize";
	}

  // Inform the batch engine that we are not finished,
  // and provide an estimation of the completion level we reached.
  if ($context['sandbox']['progress'] != $context['sandbox']['max']) {
    $context['finished'] = $context['sandbox']['progress'] / $context['sandbox']['max'];
  }

	return $local_archive;
}

function _sitepuller_tools_build_remote_backup($task_nid, $remote_subsite, $local_subsite) {
	$sitepuller_tools_message = array();
	$node = node_load($task_nid);
	$task_wrapper = entity_metadata_wrapper('node', $node);
	$pull_databases = $task_wrapper->field_sitepuller_pull_database->value();
	$pull_code = $task_wrapper->field_sitepuller_pull_code->value();
	$pull_public = $task_wrapper->field_sitepuller_pull_files->value();
	$pull_private = $task_wrapper->field_sitepuller_pull_private->value();

	$remote_site_nid = $task_wrapper->field_sitepuller_task_remo_site->getIdentifier();
	$remote_site = node_load($remote_site_nid);
	array_push($sitepuller_tools_message, _sitepuller_tools_remote_site_check($remote_site));
	$remote_site_wrapper = entity_metadata_wrapper('node', $remote_site);
	$remote_machine =  $remote_site_wrapper->field_sitepuller_remote_host->value();
	$remote_user = $remote_site_wrapper->field_sitepuller_remote_user->value();
	$remote_root = $remote_site_wrapper->field_sitepuller_remote_root->value();

	$local_site_nid = $task_wrapper->field_sitepuller_task_local_site->getIdentifier();
	$local_site = node_load($local_site_nid);
	array_push($sitepuller_tools_message, _sitepuller_tools_local_site_check($local_site));
	$local_site_wrapper = entity_metadata_wrapper('node', $local_site);
	$local_site_root = $local_site_wrapper->field_sitepuller_local_site_root->value();

	$ssh = _sitepuller_tools_get_ssh($remote_machine, $remote_user);
	$remote_temp = _sitepuller_tools_prepare_remote_temp($ssh, $remote_site_wrapper);

	if ($pull_code) {
		//drupal core
		$temp_path = _sitepuller_tools_prepare_remote_temp($ssh, $remote_site_wrapper, "code");
		_sitepuller_tools_backup_code($ssh, $remote_root, $temp_path);
	}

	if ($pull_databases) {
		// pull databases for specified subsites
		$temp_path = _sitepuller_tools_prepare_remote_temp($ssh, $remote_site_wrapper, "db/$local_subsite");
		$subsite_path = "$remote_root/sites/$remote_subsite";
		_sitepuller_tools_backup_db($ssh, $subsite_path, $temp_path);
	}

	if ($pull_public) {
		$temp_path = _sitepuller_tools_prepare_remote_temp($ssh, $remote_site_wrapper, "code/sites/$local_subsite/files");
		$subsite_path = "$remote_root/sites/$remote_subsite";
		_sitepuller_tools_backup_subsite_public_files($ssh, $remote_root, $subsite_path, $temp_path);
	}

	if ($pull_private) {
		$temp_path = _sitepuller_tools_prepare_remote_temp($ssh, $remote_site_wrapper, "code/sites/$local_subsite/private/files");
		$subsite_path = "$remote_root/sites/$remote_subsite";
		_sitepuller_tools_backup_subsite_private_files($ssh, $remote_root, $subsite_path, $temp_path);
	}

	$cmd = array(
		"test -d $remote_temp"
		);
	$out = _sitepuller_tools_ssh_cmds($ssh, $cmd);

	return $remote_temp;
}

function _sitepuller_tools_rsync_remote_to_local($task_nid, $remote_temp, $local_subsite) {
	try {
		$node = node_load($task_nid);
		$task_wrapper = entity_metadata_wrapper('node', $node);

		$remote_site_nid = $task_wrapper->field_sitepuller_task_remo_site->getIdentifier();
		$remote_site = node_load($remote_site_nid);
		$remote_site_wrapper = entity_metadata_wrapper('node', $remote_site);
		$remote_machine =  $remote_site_wrapper->field_sitepuller_remote_host->value();
		$remote_user = $remote_site_wrapper->field_sitepuller_remote_user->value();
		$remote_root = $remote_site_wrapper->field_sitepuller_remote_root->value();

		$local_site_nid = $task_wrapper->field_sitepuller_task_local_site->getIdentifier();
		$local_site = node_load($local_site_nid);
		$local_site_wrapper = entity_metadata_wrapper('node', $local_site);
		$local_site_root = $local_site_wrapper->field_sitepuller_local_site_root->value();
		$local_temp = _sitepuller_tools_prepare_local_temp($local_site_wrapper);
		$local_user = variable_get('sitepuller_tools_user', '');

		// see http://stackoverflow.com/questions/3504945/timeout-command-on-max-os-x
		// bres install coreutils
		// then use gtimeout
		$timeout = 60 * 30; // seconds
		$cmd = array (
			"gtimeout $timeout whoami",
			);
		$out = _sitepuller_tools_local_cmds($cmd);

		$cmd = array (
			"rsync -acz $remote_user@$remote_machine:$remote_temp/ $local_temp/",
			);
		$out = _sitepuller_tools_local_cmds($cmd);

		$cmd = array (
			"rsync -acz --exclude=/.htaccess --exclude=sites --delete $local_temp/code/ $local_site_root/",
			"rsync -acz --exclude=sites/*/settings.php --include=sites/$local_subsite --include=sites/all --include=sites/default/files --exclude=sites/* $local_temp/code/sites/ $local_site_root/sites/",
			);
		$out = _sitepuller_tools_local_cmds($cmd);
	}
	catch (Exception $e) {
		drupal_set_message(t("rsync remote to local failed."), 'warning', FALSE);
		throw $e;
	}
}

function _sitepuller_tools_replace_local_db($task_nid, $local_subsite) {
	try {
		$node = node_load($task_nid);
		$task_wrapper = entity_metadata_wrapper('node', $node);
		$local_site_nid = $task_wrapper->field_sitepuller_task_local_site->getIdentifier();
		$local_site = node_load($local_site_nid);
		$local_site_wrapper = entity_metadata_wrapper('node', $local_site);
		$local_site_root = $local_site_wrapper->field_sitepuller_local_site_root->value();
		$local_temp = _sitepuller_tools_prepare_local_temp($local_site_wrapper);

		$preamble = variable_get('sitepuller_tools_preamble', '');
		$cmd = array (
			"$preamble",
			"cd $local_site_root/sites/$local_subsite",
			"drush sql-drop",
			"drush sql-cli < $local_temp/db/$local_subsite/database.sql",
			"drush vset file_directory_path sites/default/files",
			"drush vset file_directory_temp /tmp",
			"drush vset file_private_path sites/default/private/files",
			"drush vset file_public_path sites/default/files",
			"drush vset file_temporary_path /tmp",
			"drush cc all",
			);
		$out = _sitepuller_tools_local_cmds($cmd);
	}
	catch (Exception $e) {
		drupal_set_message(t("rsync via exec failed."), 'warning', FALSE);
	}

}

function _sitepuller_tools_rsync_exec($remote_user, $remote_machine, $remote_temp, $local_temp, $local_site_root) {
	$preamble = variable_get('sitepuller_tools_preamble', '');
	$cmd = array (
		"rsync -asz --delete -e ssh $remote_user@$remote_machine:$remote_temp/ $local_temp/",
		"rsync -acz --exclude=sites/*/settings.php $local_temp/code/ $local_site_root/",
		"$preamble",
		"cd $local_site_root",
		"drush sql-drop",
		"drush sql-cli < $local_temp/db/default/database.sql",
		"drush vset file_directory_path sites/default/files",
		"drush vset file_directory_temp /tmp",
		"drush vset file_private_path sites/default/private/files",
		"drush vset file_public_path sites/default/files",
		"drush vset file_temporary_path /tmp",
		"drush cc all",
		);
	try {
		$out = _sitepuller_tools_local_cmds($cmd);
	}
	catch (Exception $e) {
		drupal_set_message(t("rsync via exec failed."), 'warning', FALSE);
	}
}

/**
 * tries to get connect string from local site - NEVER WORKED FOR DEVDESKTOP!
 * @param  [type] $local_site_root [description]
 * @return [type]                  [description]
 */
function _sitepuller_tools_get_db_connect_string($local_site_root) {
	try {
		$preamble = variable_get('sitepuller_tools_preamble', '');
		$cmd = array (
			"$preamble",
			"cd $local_site_root",
			//"which drush",
			"drush sql-connect",
			//"drush status --show-passwords",
		);
		$out = _sitepuller_tools_local_cmds($cmd);
	}
	catch (Exception $e) {
		drupal_set_message(t("Can not get sql connect string for @site",
			array ('@site' => $local_site_root)), 'warning', FALSE);
	}
}

function _sitepuller_tools_local_cmds($cmds, $error_message = FALSE) {
	$output = array();
	$status = 0;
	$one_cmd = implode(" ; ", $cmds);
	$out = exec($one_cmd, $output, $status);
	if ($status != 0) {
		if ($error_message === FALSE) {
			$error_message = t("Error on cmd: '@step' ; exit '@exit'",
				array('@step' => $one_cmd, '@exit' => $status));
		}
		throw new Exception($error_message);
		}
	return $output;
}

function _sitepuller_tools_ssh_cmds($ssh, $cmds, $error_message = FALSE) {
	foreach ($cmds as $step) {
		$ssh->enableQuietMode();
		$output[] = $ssh->exec($step);
		$status = $ssh->getExitStatus();
		$timeout = $ssh->isTimeout();
		$err = $ssh->getStdError();
		$ssh->disableQuietMode();
		if ($status != 0) {
			$msg = $error_message ? $error_message : t("Error on step: '@step' ; exit '@exit' ; error '@err' ",
				array('@step' => $step, '@exit' => $status, '@err' => $err));
			throw new Exception($msg);
		}
		if ($timeout) {
			throw new Exception(t("Timeout on step: '@step' ; exit '@exit' ; error '@err' ",
				array('@step' => $step, '@exit' => $status, '@err' => $err)));
		}
	}
	return $output;
}

//obsolete
function _sitepuller_tools_backup_db($ssh, $site, $directory) {
	$cmd[] = "cd $site ; drush sql-dump --ordered-dump > $directory/database.sql";
	_sitepuller_tools_ssh_cmds($ssh, $cmd);
}

function _sitepuller_tools_backup_code($ssh, $site, $directory) {
	// use . to also grab .htaccess
	$cmd[] = "shopt -s dotglob ; rsync -avcz --delete --exclude=sites/*/settings.php --exclude=sites/*/files --exclude=sites/*/private $site/ $directory/";
	//$cmd[] = "rsync -avcz --delete $site/sites/all $directory/sites/";
	_sitepuller_tools_ssh_cmds($ssh, $cmd);
}

//obsolete
function _sitepuller_tools_backup_subsite_public_files($ssh, $site_path, $subsite_path, $temp_directory) {
	$cmd = "cd $subsite_path ; drush vget file_public_path";
	$out7 = $ssh->exec($cmd);
	$match = array();
	preg_match("/file_public_path: \"([^\"]+)\"/", $out7, $match);
	if (empty($match)) {
		$cmd = "cd $subsite_path ; drush vget file_directory_path";
		$out6 = $ssh->exec($cmd);
		$match = array();
		preg_match("/file_directory_path: \"([^\"]+)\"/", $out6, $match);
		if (empty($match)) {
			throw new Exception(t("Failed to determine public file path for @s in @p", array('@s' => $subsite_path, '@p' => $out6)));
		}
	}
	$path = $site_path . '/' . $match[1];
	$target = $temp_directory;
	$cmd = array();
	$cmd[] = "mkdir -p $target";
	$cmd[] = "rsync -avcz --delete $path/ $target/";
	_sitepuller_tools_ssh_cmds($ssh, $cmd, t("Can't copy public files for subsite @p", array('@p' => $subsite_path)));
}

// obsolete
function _sitepuller_tools_backup_subsite_private_files($ssh, $site_path, $subsite_path, $temp_directory) {
	$cmd = "cd $subsite_path ; drush vget file_private_path";
	$out7 = $ssh->exec($cmd);
	$match = array();
	preg_match("/file_private_path: \"([^\"]+)\"/", $out7, $match);
	if (empty($match)) {
		throw new Exception(t("Failed to determine private file path for @s", array('@s' => $subsite_path)));
		}
	$path = $site_path . '/' . $match[1];
	$target = $temp_directory;
	$cmd = array();
	$cmd[] = "mkdir -p $target";
	$cmd[] = "shopt -s dotglob ; rsync -avcz --delete $path/ $target/";
	_sitepuller_tools_ssh_cmds($ssh, $cmd, t("Can't copy private files for subsite @p", array('@p' => $subsite_path)));
}

/**
 * encrypt a string, return hex encoded version of encryption
 * @param  string $enropy     file contents of key file
 * @param  string $passphrase any string
 * @return string             hex encoded encrypted string
 */
function _sitepuller_tools_encrypt($entropy, $passphrase) {
  $hash = sha1($entropy);  // returns 40 character hex
  $cipher = new Crypt_Blowfish();
  // keys can range in length from 32 bits to 448 in steps of 8
  $cipher->setKey($hash);
  $encrypted = $cipher->encrypt($passphrase);
  $unpacked = bin2hex($encrypted);
  return $unpacked;
}

/**
 * quick check to see if user is configured
 * @return nothing throws exception if there is no user configuration
 */
function _sitepuller_tools_check_user() {
	$encrypted_password = variable_get('sitepuller_tools_passphrase');
	if (empty($encrypted_password)) {
			throw new Exception(t("Please go to @path to configure your local user account for Sitepuller.",
				array('@path' => 'admin/settings/sitepuller_tools',)
				)
			);
	}
}

function _sitepuller_tools_local_subsites($local_site_root) {
	$cmd[] = "cd $local_site_root";
	$cmd[] = "ls sites/*/settings.php";
	$out = _sitepuller_tools_local_cmds($cmd);
	$subsites = array();
	foreach ($out as $value) {
		$parts = explode('/', $value);
		if (!empty($parts[1])) {
			$subsites[] = $parts[1];
		}
	}
	return $subsites;
}

function _sitepuller_tools_remote_subsites($ssh, $remote_site_root) {
	$cmd[] = "cd $remote_site_root";
	$cmd[] = "ls sites/*/settings.php";
	$out = _sitepuller_tools_ssh_cmds($ssh, $cmd);
	$subsites = array();
	foreach ($out as $value) {
		$parts = explode('/', $value);
		if (!empty($parts[1])) {
			$subsites[] = $parts[1];
		}
	}
	return $subsites;
}

function _sitepuller_tools_cache_remote_site($nid, $name, $mach, $subsites) {
	$cache = _sitepuller_tools_get_cached_data();
	$cache['remote']["$nid"] =  array('name' => $name, 'machine' => $mach, 'subsites' => $subsites);
	_sitepuller_tools_set_cached_data($cache);
}

function _sitepuller_tools_cache_local_site($nid, $name, $subsites) {
	$cache = _sitepuller_tools_get_cached_data();
	$cache['local']["$nid"] =  array('name' => $name, 'subsites' => $subsites);
	_sitepuller_tools_set_cached_data($cache);
}

function _sitepuller_tools_get_cached_data($new_data = FALSE) {
	if ($new_data) {
		cache_set('sitepuller_tools_data', $new_data, 'cache');
		$my_data = $new_data;
	}
	else {
		$my_data = &drupal_static(__FUNCTION__);
		if (!isset($my_data)) {
			if ($cache = cache_get('sitepuller_tools_data')) {
				$my_data = $cache->data;
			}
			else {
				$my_data = array();
				cache_set('sitepuller_tools_data', $my_data, 'cache');
			}
		}
	}
	return $my_data;
}

function _sitepuller_tools_set_cached_data($my_data) {
	$new_data = _sitepuller_tools_get_cached_data($my_data);
}

function _sitepuller_tools_store_cmds($machine, $cmds) {
	global $sitepuller_cmds;
	foreach($cmds as $cmd) {
		$sitepuller_cmds["$machine"][] = $cmd;
	}
	return $stop = TRUE;
}

function _sitepuller_tools_write_cmds($temp_dir) {
	global $sitepuller_cmds;
	dsm ($sitepuller_cmds);
	foreach ($sitepuller_cmds as $machine => $cmds) {
		$text = implode("/n", $cmds);
		$file = "$temp_dir/{$machine}_commands.sh";
		file_put_contents($file, $text);
	}
}

function _sitepuller_tools_info_task($task_nid) {
	$info = array();
	$node = node_load($task_nid);
	$task_wrapper = entity_metadata_wrapper('node', $node);
	$info['local_db_connect'] =  $task_wrapper->field_sitepuller_local_db_connec->value();
	$info['local_site_nid'] = $task_wrapper->field_sitepuller_task_local_site->getIdentifier();
	$info['local_subsite'] =  $task_wrapper->field_sitepuller_task_local_sub->value();
	$info['private_files_path'] =  $task_wrapper->field_sitepuller_private_files->value();
	$info['public_files_path'] =  $task_wrapper->field_sitepuller_public_files->value();
	$info['pull_code'] =  $task_wrapper->field_sitepuller_pull_code->value();
	$info['pull_database'] =  $task_wrapper->field_sitepuller_pull_database->value();
	$info['pull_drupal'] =  $task_wrapper->field_sitepuller_pull_drupal->value();
	$info['pull_files'] =  $task_wrapper->field_sitepuller_pull_files->value();
	$info['pull_private'] =  $task_wrapper->field_sitepuller_pull_private->value();
	$info['remote_db_connect'] =  $task_wrapper->field_sitepuller_remote_db->value();
	$info['remote_site_nid'] = $task_wrapper->field_sitepuller_task_remo_site->getIdentifier();
	$info['remote_subsite'] =  $task_wrapper->field_sitepuller_task_remo_sub->value();
	return $info;
}

function _sitepuller_tools_info_remote($remote_site_nid) {
	$info = array();
	$node = node_load($remote_site_nid);
	$wrapper = entity_metadata_wrapper('node', $node);
	$info['host'] = $wrapper->field_sitepuller_remote_host->value();
	$info['root'] = $wrapper->field_sitepuller_remote_root->value();
	$info['site_name'] = $wrapper->field_sitepuller_remote_site->value();
	$info['tmp'] = $wrapper->field_sitepuller_remote_tmp->value();
	$info['user'] = $wrapper->field_sitepuller_remote_user->value();
	$info['stash_dir'] = $info['tmp'] . '/sitepuller/' . $info['site_name'];
	$info['stash_name'] = 'stash';
	$info['stash'] =  $info['stash_dir'] . '/' . $info['stash_name'];
	$info['tarball_name'] = $info['stash_name'] . ".tar.gz";
	$info['tarball'] = $info['stash_dir'] . '/' . $info['tarball_name'];
	return $info;
	}

function _sitepuller_tools_info_local($local_site_nid) {
	$info = array();
	$node = node_load($local_site_nid);
	$wrapper = entity_metadata_wrapper('node', $node);
	$info['drush_make_file'] = $wrapper->field_sitepuller_drush_make_file->value();
	$info['site_name'] = $wrapper->field_sitepuller_local_site_name->value();
	$info['site_root'] = $wrapper->field_sitepuller_local_site_root->value();
	$info['tmp'] = $wrapper->field_sitepuller_local_tmp->value();
	$info['use_make_file'] = $wrapper->field_sitepuller_use_make_file->value();
	$info['stash_dir'] = $info['tmp'] . '/sitepuller/' . $info['site_name'];
	$info['stash_name'] = 'stash';
	$info['stash'] =  $info['stash_dir'] . '/' . $info['stash_name'];
	$info['tarball_name'] = $info['stash_name'] . ".tar.gz";
	$info['tarball'] = $info['stash_dir'] . '/' . $info['tarball_name'];
	return $info;
}

