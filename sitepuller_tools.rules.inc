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
 *	user_node - loaded node - local sitepuller user
 *
 *  returns Net_SSH2 ssh object or FALSE
 */
function _sitepuller_tools_get_ssh($machine, $remote_user, $user_node) {
	$wrapper = entity_metadata_wrapper('node', $user_node);
	$rsa_private = $wrapper->field_sitepuller_rsa_private_key->value();
	$passphrase = $wrapper->field_sitepuller_rsa_passphrase->value();
	$entropy = $wrapper->field_sitepuller_encrypt_entropy->value();

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

function sitepuller_tools_action_check_remote_site($node) {
	$wrapper = entity_metadata_wrapper('node', $node);

	$user = $wrapper->field_sitepuller_remote_user->value();
	$mach = $wrapper->field_sitepuller_remote_host->value();
	$path = $wrapper->field_sitepuller_remote_root->value();
	$module = drupal_get_path('module', 'sitepuller');
	$scripts =  drupal_realpath($module . '/sh');
	$cmd = "/bin/bash $scripts/remote_dir_check.sh $user $mach $path";
	$cmd = "ssh $user@$mach test -d \"$path\" || exit 1";
	$cmd = escapeshellcmd($cmd);
	//$cmd = "which ssh";
	$output='';
	$error='';
	//exec($cmd, $output, $error);
	dsm (array('the node is' => $node, 'command' => $cmd, 'error' => $error, 'output' => $output));


$ssh = new Net_SSH2("$mach");
if ($ssh->isConnected()) dsm('connected 1');
// keyfile must be id_rsa (private key) - absolute path
$keyfile = file_get_contents('/Users/jgreidy/.ssh/id_rsa');
if (empty($keyfile)) {
	dsm('no keyfile');
	return;
}
$key = new Crypt_RSA();
// password must be the one set for id_rsa above
$key->setPassword('xxx');
$key->loadKey($keyfile);
if (!$ssh->login($user,$key)) {
	$allErr = $ssh->getErrors();
    dsm(array('Login Failed', $allErr));
    return;
}

if ($ssh->isConnected()) dsm('connected 2');

$pwd = $ssh->exec('pwd');
$err1 = $ssh->getStdError();
$ls = $ssh->exec("test -d \"$path\" || echo 'no path'");
$err2 = $ssh->getStdError();
$allErr = $ssh->getErrors();
dsm(array($pwd, $err1, $ls, $err2, $allErr));

}

function sitepuller_tools_action_check_local_site($node) {
	dsm('hello from sitepuller_tools_action_check_local_site');
}

function _sitepuller_tools_prepare_remote_temp($ssh, $remote_site_wrapper, $directory) {
	$tmp = $remote_site_wrapper->field_sitepuller_remote_tmp->value();
	$user = $remote_site_wrapper->field_sitepuller_remote_user->value();
	$site = $remote_site_wrapper->field_sitepuller_remote_site->value();
	$path = "$tmp/sitepuller/$site/$user/$directory";
	$cmd[] = "mkdir -p $path";
	$cmd[] = "test -d $path";
	_sitepuller_tools_ssh_cmds($ssh, $cmd);
	return $path;
}

function sitepuller_tools_action_save_task($node) {
	$sitepuller_tools_message = array();
	try {
		dsm('hello from sitepuller_tools_action_save_task');
		$task_wrapper = entity_metadata_wrapper('node', $node);
		$subsites = $task_wrapper->field_sitepuller_subsites->value();
		if (empty($subsites)) {
			throw new Exception(t("Doing all subsites is not working yet. Please specify one or more subsites"));
		}
		$pull_databases = $task_wrapper->field_sitepuller_pull_database->value();
		$pull_code = $task_wrapper->field_sitepuller_pull_code->value();
		$pull_public = $task_wrapper->field_sitepuller_pull_files->value();
		$pull_private = $task_wrapper->field_sitepuller_pull_private->value();

		$remote_site_nid = $task_wrapper->field_sitepuller_task_remo_site->getIdentifier();
		$remote_site = node_load($remote_site_nid);
		$remote_site_wrapper = entity_metadata_wrapper('node', $remote_site);
		$machine =  $remote_site_wrapper->field_sitepuller_remote_host->value();
		$remote_user = $remote_site_wrapper->field_sitepuller_remote_user->value();
		$remote_root = $remote_site_wrapper->field_sitepuller_remote_root->value();

		$local_site_nid = $task_wrapper->field_sitepuller_task_local_site->getIdentifier();
		$local_site = node_load($local_site_nid);
		$local_site_wrapper = entity_metadata_wrapper('node', $local_site);

		$local_user_nid = $task_wrapper->field_local_user->getIdentifier();
		$local_user = node_load($local_user_nid);

		$ssh = _sitepuller_tools_get_ssh($machine, $remote_user, $local_user);

		// if ($pull_code) {
		// 	//drupal core
		// 	$temp_path = _sitepuller_tools_prepare_remote_temp($ssh, $remote_site_wrapper, "code");
		// 	_sitepuller_tools_backup_code($ssh, $remote_root, $temp_path);
		// }

		// if ($pull_databases) {
		// 	// pull databases for specified subsites
		// 	foreach ($subsites as $delta => $subsite) {
		// 		$temp_path = _sitepuller_tools_prepare_remote_temp($ssh, $remote_site_wrapper, "db/$subsite");
		// 		$subsite_path = "$remote_root/sites/$subsite";
		// 		_sitepuller_tools_backup_db($ssh, $subsite_path, $temp_path);
		// 	}
		// }

		if ($pull_public) {
			foreach ($subsites as $delta => $subsite) {
				$temp_path = _sitepuller_tools_prepare_remote_temp($ssh, $remote_site_wrapper, "code/sites/$subsite");
				$subsite_path = "$remote_root/sites/$subsite";
				_sitepuller_tools_backup_subsite_public_files($ssh, $subsite_path, $temp_path);
			}

		}

		dsm(array('site' => $remote_site_nid, 'user' => $local_user_nid));
	}
	catch (Exception $e) {
		$sitepuller_tools_message[] = $e->getMessage();
		return array (
			'sitepuller_tools_message' => implode("\r",$sitepuller_tools_message),
			);
	}
}

function _sitepuller_tools_ssh_cmds($ssh, $cmds) {
	dsm($cmds);
	foreach ($cmds as $step) {
		$ssh->enableQuietMode();
		$out = $ssh->exec($step);
		$status = $ssh->getExitStatus();
		$timeout = $ssh->isTimeout();
		$err = $ssh->getStdError();
		$ssh->disableQuietMode();
		if ($status != 0) {
			throw new Exception(t("Error on step: '@step' ; exit '@exit' ; error '@err' ",
				array('@step' => $step, '@exit' => $status, '@err' => $err)));
		}
		if ($timeout) {
			throw new Exception(t("Timeout on step: '@step' ; exit '@exit' ; error '@err' ",
				array('@step' => $step, '@exit' => $status, '@err' => $err)));
		}
	}
}

function _sitepuller_tools_backup_db($ssh, $site, $directory) {
	$cmd[] = "rm -f $directory/database.sql";
	$cmd[] = "drush sql-dump --root=$site --ordered-dump > $directory/database.sql";
	_sitepuller_tools_ssh_cmds($ssh, $cmd);
}

function _sitepuller_tools_backup_code($ssh, $site, $directory) {
	$cmd[] = "rsync -avcz --delete --exclude=sites/* $site/ $directory/";
	$cmd[] = "rsync -avcz --delete $site/sites/all $directory/sites/";
	_sitepuller_tools_ssh_cmds($ssh, $cmd);
}

function _sitepuller_tools_backup_subsite_public_files($ssh, $site, $directory) {
	$cmd = "cd $site ; drush vget file_public_path";
	$out7 = $ssh->exec($cmd);
	$cmd = "cd $site ; drush vget file_directory_path";
	$out6 = $ssh->exec($cmd);
	dsm (array("d7 file_public_path" => $out7, "d6 file_directory_path" => $out6));
}
