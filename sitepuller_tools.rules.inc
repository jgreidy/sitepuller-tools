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

function _sitepuller_tools_remote_site_check($node) {
	$wrapper = entity_metadata_wrapper('node', $node);

	$user = $wrapper->field_sitepuller_remote_user->value();
	$mach = $wrapper->field_sitepuller_remote_host->value();
	$path = $wrapper->field_sitepuller_remote_root->value();
	$name = $wrapper->field_sitepuller_remote_site->value();

	$ssh = _sitepuller_tools_get_ssh($mach, $user);
	$cmd[] = "test -d $path";
	$output = _sitepuller_tools_ssh_cmds($ssh, $cmd, t("Directory does not exist: @d", array("@d" => $path)));
	$cmd[] = "cd $path ; drush core-status --pipe | grep bootstrap | grep Successful";
	$output = _sitepuller_tools_ssh_cmds($ssh, $cmd, t("Can't connect to Drupal database for site '@name'", array('@name' => $name)));
	$directory = "test";
	_sitepuller_tools_prepare_remote_temp($ssh, $wrapper, $directory);
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
	$connect = $wrapper->field_sitepuller_local_db_connec->value();
	$use_make = $wrapper->field_sitepuller_use_make_file->value();
	$make_file = $wrapper->field_sitepuller_drush_make_file ->value();
	$php = variable_get('sitepuller_tools_php_path', 'php');
	$drush = variable_get('sitepuller_tools_drush_path', 'drush');
	$mysql = variable_get('sitepuller_tools_mysql_path', 'mysql');

	if ($use_make != 0) {
		$cmd[] = "test -d $path || drush make --prepare-install --working-copy $make_file $path";
		$output = _sitepuller_tools_local_cmds($cmd, t('drush make failed: @dir', array('@dir' => $make_file)));
	}

	$cmd[0] = "test -d $path";
	$output = _sitepuller_tools_local_cmds($cmd, t('directory does not exist: @dir', array('@dir' => $path)));
	$cmd[0] = str_replace('mysql', $mysql, $connect) . " -e 'exit'";
	$output = _sitepuller_tools_local_cmds($cmd, t('Can not connect to the database for site \'@name\'', array('@name' => $name)));
	$directory = "test";
	_sitepuller_tools_prepare_local_temp($wrapper, $directory);
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

function sitepuller_tools_action_save_task($node) {
	$sitepuller_tools_message = array();
	try {
		_sitepuller_tools_check_user();
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
		$local_db_connect = $local_site_wrapper->field_sitepuller_local_db_connec->value();

		$ssh = _sitepuller_tools_get_ssh($remote_machine, $remote_user);
		$remote_temp = _sitepuller_tools_prepare_remote_temp($ssh, $remote_site_wrapper);

		if ($pull_code) {
			//drupal core
			$temp_path = _sitepuller_tools_prepare_remote_temp($ssh, $remote_site_wrapper, "code");
			_sitepuller_tools_backup_code($ssh, $remote_root, $temp_path);
		}

		if ($pull_databases) {
			// pull databases for specified subsites
			foreach ($subsites as $delta => $subsite) {
				$temp_path = _sitepuller_tools_prepare_remote_temp($ssh, $remote_site_wrapper, "db/$subsite");
				$subsite_path = "$remote_root/sites/$subsite";
				_sitepuller_tools_backup_db($ssh, $subsite_path, $temp_path);
			}
		}

		if ($pull_public) {
			foreach ($subsites as $delta => $subsite) {
				$temp_path = _sitepuller_tools_prepare_remote_temp($ssh, $remote_site_wrapper, "code/sites/$subsite/files");
				$subsite_path = "$remote_root/sites/$subsite";
				_sitepuller_tools_backup_subsite_public_files($ssh, $remote_root, $subsite_path, $temp_path);
			}
		}

		if ($pull_private) {
			foreach ($subsites as $delta => $subsite) {
				$temp_path = _sitepuller_tools_prepare_remote_temp($ssh, $remote_site_wrapper, "code/sites/$subsite/private/files");
				$subsite_path = "$remote_root/sites/$subsite";
				_sitepuller_tools_backup_subsite_private_files($ssh, $remote_root, $subsite_path, $temp_path);
			}
		}

		// copy the site to the local machine
		$local_temp = _sitepuller_tools_prepare_local_temp($local_site_wrapper);
		$local_user = variable_get('sitepuller_tools_user', '');
		$cmd[] = "hostname";
		$local_machine = _sitepuller_tools_local_cmds($cmd);
		$local_machine = array_pop($local_machine);
		unset($cmd);
		_sitepuller_tools_rsync_exec($remote_user, $remote_machine, $remote_temp, $local_temp, $local_site_root,
			$local_db_connect);
	}
	catch (Exception $e) {
		$sitepuller_tools_message[] = $e->getMessage();
		return array (
			'sitepuller_tools_message' => implode("\r ",$sitepuller_tools_message),
			);
	}
}

function _sitepuller_tools_rsync_exec($remote_user, $remote_machine, $remote_temp, $local_temp, $local_site_root,
	$local_db_connect) {
	$cmd = array (
		"rsync -asz --delete -e ssh $remote_user@$remote_machine:$remote_temp/ $local_temp/",
		"rsync -acz --exclude=sites/*/settings.php $local_temp/code/ $local_site_root/",
		'export PHP_ID=php5_5; export PATH="/Applications/DevDesktop/$PHP_ID/bin:/Applications/DevDesktop/mysql/bin:/Applications/DevDesktop/drush:$PATH"',
		"cd $local_site_root",
		"$local_db_connect < $local_temp/db/default/database.sql",
		"drush vset file_directory_path sites/default/files",
		"drush vset file_directory_temp /tmp",
		"drush vset file_private_path sites/default/private/files",
		"drush vset file_public_path sites/default/files",
		"drush vset file_temporary_path /tmp",
		"drush cc all",
		);
	try {
		$out = _sitepuller_tools_local_cmds($cmd);
		dsm($out);
	}
	catch (Exception $e) {
		drupal_set_message(t("rsync via exec failed."), 'warning', FALSE);
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
			throw new Exception(t("Failed to determine public file path for @s", array('@s' => $subsite_path)));
		}
	}
	$path = $site_path . '/' . $match[1];
	$target = $temp_directory;
	$cmd = array();
	$cmd[] = "mkdir -p $target";
	$cmd[] = "rsync -avcz --delete $path/ $target/";
	_sitepuller_tools_ssh_cmds($ssh, $cmd, t("Can't copy public files for subsite @p", array('@p' => $subsite_path)));
}

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

function _sitepuller_tools_local_drupal_version() {

}

