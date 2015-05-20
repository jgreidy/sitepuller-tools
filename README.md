# sitepuller_tools

Module to support Sitepuller Feature

Install sitepuller:
- see the instructions in git@github.com:jgreidy/sitepuller-profile.git

Configure sitepuller:
- go into the drupal site and go to admin/modules
- go to Sitepuller Tools -> Permissions and give yourself all permissions for
  - Sitepuller Local
  - Sitepuller Remote
  - Sitepuller Task
- go to admin/settings/sitepuller_tools for the user configuration
  - fill out the form describing your local user and machine
  - you only need to do this once

Create a node describing your local site:
- create the local site
  - you can use drush make on the command line
  - you can use DevDesktop to create a simple latest version of Drupal site
  - you can use the Sitepuller Local Site node
    - enter a drush make file path in the sitepuller local site node
    - check off "Create an empty site using a drush make file"
    - the first time you save the node, it will create a site from your drush make file
- got to the new site in your browser and run through the Drupal initialization to connect it to it's database
  - you need to create the database yourself with MAMP
  - DevDesktop creates the database for you
- go to node/add (Create Content) and create a Sitepuller Local node
- whenever you save this node sitepuller confirms that it can work with this site

Create a node describing your remote site:
- go to Create Content on the drupal site and create a Sitepuller Remote node
  - this describes a site you want to copy on some remote machine
  - you must be set up to ssh to that machine without a password prompt
- whenever you save this node sitepuller confirms that it can work with this site

Create a Sitepuller Task to copy the remote site to the local one
- go to Create Content on the drupal site and create a Sitepuller Task node
- select the remote site as a source, and a local site as a destination
- choose which parts of the site to copy

