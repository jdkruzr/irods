#!/usr/bin/python

from datetime import *
import sys, re, os 
import subprocess as sp
from nose.tools import assert_equal
from nose.tools import assert_not_equal

getent_path = "/home/samuel/wksp/irods/scripts/getent"

class SyncRunner(object):
    def __init__(self):
        self.users = []
        # Info types to sync, from LDAP
        self.infotypes = ["shadowExpire", "uid"]
        self.skipusers_patterns = ["grid"] 
        
    def run(self):
        '''
        The main method of the script
        '''
        
        # Parse LDAP data to user objects
        ldapdata = self.get_userinfo_from_ldap(self.infotypes)
        users = self.parse_ldap_data_to_users(ldapdata)
        users = self.filter_usernames(users)
        users = self.filter_expired_users(users)
        self.users = users
        
        # Get an iRODS connector to talk to iRODS
        irods = IRodsConnector()
        
        # Create users
        for user in self.users:
            if not irods.user_exists(user.username):
                #sys.stderr.write("User %s missing, so creating now ...\n" % user.username)
                irods.create_user(user.username, usertype="rodsuser")
                
        # Get all groups
        groups = self.get_groups()

        # Create groups
        for group in groups:
            if not irods.group_exists(group.groupname):
                #sys.stderr.write("Group %s missing, so creating now ...\n" % group.groupname)
                irods.create_group(group.groupname)

        # Connect users and groups
        for group in groups:
            for username in group.usernames:
                if irods.user_exists(username):
                    sys.stderr.write("Now adding user %s to group %s ...\n" % (username,group.groupname))
                    irods.add_user_to_group(username, group.groupname)
                    

                
        #for group in groups:
        #    print "Group: " + group.groupname
            
        #connect_users_and_groups() {
        #  for group in $(getent group|grep -P "^(a|b)20"|cut -f1 -d:); do
        #    if [[ "" == `iadmin lg|grep $group` ]]; then
        #      echo "Group $group missing, so creating now ...";
        #      iadmin mkgroup $group;
        #    fi
        #
        #    # Create the proj folder, if missing
        #    projfolder="/ssUppnexZone/proj/$group";
        #    if [[ $(ils $projfolder 2>&1) == ERROR* ]]; then
        #      echo "Creating folder for group $group ...";
        #      # Create folder
        #      imkdir $projfolder;
        #      # Make the group to own the project folder
        #      ichmod own $group $projfolder;
        #      # Prevent access for "anyone"
        #      ichmod null public $projfolder;
        #    fi
        #     for username in $(getent group|grep $group|tr ":" "\n"|tail -n +4|tr "," "\n"); do
        #      if [[ "" == `iadmin lg $group|grep $username` ]]; then
        #        if [[ -n `iadmin lu|grep -P "^$username#"` ]]; then
        #          echo "Valid user $username missing from $group, so adding now ...";
        #          iadmin atg $group $username;
        #        fi
        #      fi
        #    done;
        #  done;
        #}

        
    

    def parse_ldap_data_to_users(self, ldapdata):
        '''
        Parse the output of the ldapsearch command to user objects
        ''' 
        users = []
        for userpart in ldapdata.split("\n\n"):
            try: 
                user = User()
                user.username = self.get_match("uid: (\w+)", 1, userpart)
                expiretime_d = int(self.get_match("shadowExpire: (\d+)", 1, userpart))
                user.expirytime = expiretime_d * 24 * 3600
                users.append(user)
            except StopIteration:
                pass

        return users
        
    def filter_usernames(self, users):
        def contains_pattern(teststring, patterns):
            contains_pattern = False
            for pattern in patterns:
                if pattern in teststring:
                    contains_pattern = True
            return contains_pattern

        filtered_users = []
        for user in users:
            # Test if username does not contain any of the skip pattern 
            if not contains_pattern(user.username, self.skipusers_patterns):
                filtered_users.append(user)
        return filtered_users     

    def filter_expired_users(self, users):
        filtered_users = []
        for user in users:
             if user.expirytime > user.get_posixtime():
                 filtered_users.append(user)
        return filtered_users

    def get_userinfo_from_ldap(self, infotypes):
        ldapcmd = "ldapsearch -x -LLL '(uid=*)'"
        for infotype in infotypes:
            ldapcmd += " %s" % infotype
        output = exec_cmd(ldapcmd)
        return output

    def get_groups(self):
        groups = []
        cmd = getent_path + " group"
        groupdata = exec_cmd(cmd)
        groupdata = groupdata.strip()
        for line in groupdata.split("\n"):
            cols = line.split(":")
            group = Group()
            group.groupname = cols[0]
            group.usernames.extend(cols[3].split(","))
            groups.append(group)
        return groups

        # |grep -P "^(a|b)20"|cut -f1 -d:)"

    def get_match(self, pattern, group, userpart):
        it = re.finditer(pattern, userpart, re.MULTILINE|re.DOTALL)
        m = it.next()
        match = m.group(group)
        return match

class User(object):
    def __init__(self):
        self.username = None
        self.expirytime = None

    def has_expired(self):
        has_expired = True
        self.now_posix = self.get_posixtime()
        if self.expirytime > self.now_posix: 
            has_expired = False
        return has_expired
    
    def get_posixtime(self):
        now = datetime.now()
        posixtime = int(now.strftime("%s"))
        return posixtime

class Group(object):
    def __init__(self):
        self.groupname = ""
        self.usernames = []

class IRodsConnector(object):
    def __init__(self):
        self.icommands_path = "/opt/irods/iRODS/clients/icommands/bin"
        # Some paths to binaries:

    def user_exists(self, username):
        cmd = self.get_iadmin_p() + " lu " + username
        output = exec_cmd(cmd)
        output = str.strip(output)
        if output == "No rows found":
            return False
        else:
            return True

    def create_user(self, username, usertype="rodsuser"):
        cmd = "%s mkuser %s %s" % (self.get_iadmin_p(), username, usertype)
        exec_cmd(cmd)
        
    def delete_user(self, username):
        cmd = "%s rmuser %s" % (self.get_iadmin_p(), username)
        exec_cmd(cmd)
        
    def list_users_in_zone(self, zone):
        cmd = self.get_iadmin_p() + " luz " + zone
        output = exec_cmd(cmd)
        users = str.strip(output).split("\n")
        return users
        
    def group_exists(self, groupname):
        cmd = self.get_iadmin_p() + " lg"
        groupdata = exec_cmd(cmd)
        if groupname in groupdata: 
            return True
        else:
            return False
        
    def create_group(self, groupname):
        cmd = self.get_iadmin_p() + " mkgroup " + groupname
        exec_cmd(cmd)
        
    def delete_group(self, groupname):
        if self.group_exists(groupname):
            cmd = self.get_iadmin_p() + " rmgroup " + groupname
            exec_cmd(cmd)
        else:
            #sys.stderr.write("Group does not existing, so can not delete: %s\n" % groupname)
            pass
            
    def add_user_to_group(self, username, groupname):
        cmd = self.get_iadmin_p() + " atg %s %s" % (groupname, username)
        exec_cmd(cmd)
        
    def get_iadmin_p(self):
        return os.path.join(self.icommands_path, "iadmin")
    

# Tests

class TestSyncRunner(object):
    @classmethod
    def setup_class(self):
        f = open("ldap_testdata.txt")
        self.testdata = f.read()
        f.close()
        self.syncrunner = SyncRunner()
        self.users = self.syncrunner.parse_ldap_data_to_users(self.testdata)
        self.expired_user = self.users[2]
        # Make one user already expired
        expired_time = self.expired_user.get_posixtime() - 3600
        self.expired_user.expirytime = expired_time 
        self.users = self.syncrunner.filter_usernames(self.users)
        self.users = self.syncrunner.filter_expired_users(self.users)
        self.delete_all_users()
        self.delete_all_groups()

    @classmethod        
    def teardown_class(self):
        self.delete_all_users()
        self.delete_all_groups()

    def test_expirydate(self):
       for user in self.users:
            assert user.expirytime > user.get_posixtime()
            
    def test_username(self):
        for user in self.users:
            assert "grid" not in user.username

    def test_epirydate2(self):
        for user in self.users:
            assert_not_equal(self.expired_user.username, user.username)
            
    def test_blackbox(self):
        self.syncrunner.run()
        
    @classmethod    
    def delete_all_users(self):
        irods = IRodsConnector()
        for user in irods.list_users_in_zone("ssUppnexZone"):
            if not "rods" in user:
                #sys.stderr.write("Now deleting user " + user + "...\n")
                irods.delete_user(user)

    @classmethod    
    def delete_all_groups(self):
        irods = IRodsConnector()
        for group in self.syncrunner.get_groups():
            if not "public" in group.groupname and not "rodsadmin" in group.groupname:
                #sys.stderr.write("Now deleting group " + group.groupname + "...\n")
                try:
                    irods.delete_group(group.groupname)
                except:
                    sys.stderr.write("Could not delete group %s\n" % group.groupname)


# Some global methods

def exec_cmd(command):
    output = ""        
    try:
        print("Now executing: " + command)
        commandlist = command.split(" ")
        p = sp.Popen(commandlist, stdout=sp.PIPE)
        output = p.stdout.read()
    except Exception:
        sys.stderr.write("ERROR: Could not execute command: " + command)
        raise
    return output

# Run main

if __name__ == "__main__":
    syncrunner = SyncRunner()
    syncrunner.run()