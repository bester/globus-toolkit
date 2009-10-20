# Copyright 1999-2009 University of Chicago
# 
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
# 
# http://www.apache.org/licenses/LICENSE-2.0
# 
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""
Utilities and objects for processing GRAM5 usage packets.
"""

from cusagepacket import CUsagePacket
from dnscache import DNSCache
import time
import re

class GRAM5Packet(CUsagePacket):
    """
    GRAM5 Usage Packet handler
    """
    
    __all_init = 0
    # Caches of key -> id maps
    dns_cache = None
    __lrms = dict()
    __job_managers = dict()
    __job_manager_instances = dict()
    __job_manager_instances_by_uuid = dict()
    clients = dict()
    executables = dict()
    __versions = dict()
    __rsl_attributes = dict()
    __rsl_bitfields = dict()
    job_type_ids = dict()
    db_class = None

    def __init__(self, address, packet):
        CUsagePacket.__init__(self, address, packet)

    
    @staticmethod
    def upload_many(dbclass, cursor, packets):
        """
        Upload many GRAM5Packet usage packets to the database referred to
        by the given cursor. It will also prepare the caches of id tables
        """
        if GRAM5Packet.__all_init == 0:
            GRAM5Packet.db_class = dbclass

            GRAM5Packet._all_init = 1
            GRAM5Packet.__init_dns_cache(cursor)
            GRAM5Packet.__init_versions(cursor)
            GRAM5Packet.__init_lrms(cursor)
            GRAM5Packet.__init_job_managers(cursor)
            GRAM5Packet.__init_job_manager_instances(cursor)
            GRAM5Packet.__init_rsl_attributes(cursor)
            GRAM5Packet.__init_rsl_bitfields(cursor)
            GRAM5Packet.__init_job_type_ids(cursor)
            GRAM5Packet.__init_clients(cursor)
            GRAM5Packet.__init_executables(cursor)
            GRAM5Packet.cursor = cursor
        CUsagePacket.upload_many(dbclass, cursor, packets)
        
    def get_job_manager_id(self, cursor):
        host_id = self.get_host_id()
        version_id = self.get_version_id(cursor)
        lrm_id = self.get_lrm_id(cursor)
        seg_used = (self.data.get('F') == '0')
        poll_used = (self.data.get('F') == '1')
        audit_used = (self.data.get('G') == '1')

        values = (host_id, version_id, lrm_id, seg_used, poll_used, audit_used)

        job_manager_id = GRAM5Packet.__job_managers.get(values)
        if job_manager_id is None:
            cursor.execute('''
                INSERT INTO gram5_job_managers(
                    host_id,
                    version,
                    lrm_id,
                    seg_used,
                    poll_used,
                    audit_used)
                VALUES(%s, %s, %s, %s, %s, %s)
                RETURNING id''', values)
            job_manager_id = cursor.fetchone()[0]
            GRAM5Packet.__job_managers[values] = job_manager_id
        return job_manager_id

    def get_job_manager_instance_id(self, cursor):
        job_manager_id = self.get_job_manager_id(cursor)
        uuid = self.data.get('B')
        start_time = GRAM5Packet.db_class.TimestampFromTicks(
                float(self.data.get('A')))
        job_manager_instance_id = \
            GRAM5Packet.__job_manager_instances_by_uuid.get(uuid)
        if job_manager_instance_id is None:
            values = (job_manager_id, uuid, start_time)
            cursor.execute('''
                INSERT INTO gram5_job_manager_instances(
                        job_manager_id,
                        uuid,
                        start_time)
                VALUES(%s, %s, %s)
                RETURNING id''', values)
            job_manager_instance_id = cursor.fetchone()[0]
            GRAM5Packet.__job_manager_instances_by_uuid[uuid] = \
                    job_manager_instance_id
        return job_manager_instance_id

    def get_job_manager_instance_id_by_uuid(self, cursor):
        uuid = self.data.get('B')
        start_time = None
        if self.data.get('A') != None:
            start_time = GRAM5Packet.db_class.TimestampFromTicks(
                    float(self.data.get('A')))
        values = (uuid, start_time)
        job_manager_instance_id = \
            GRAM5Packet.__job_manager_instances_by_uuid.get(uuid)
        if job_manager_instance_id is None:
            cursor.execute('''
                INSERT INTO gram5_job_manager_instances(
                        uuid,
                        start_time)
                VALUES(%s, %s)
                RETURNING id''', values)
            job_manager_instance_id = cursor.fetchone()[0]
            GRAM5Packet.__job_manager_instances[values] = \
                    job_manager_instance_id
            GRAM5Packet.__job_manager_instances_by_uuid[uuid] = \
                    job_manager_instance_id
        return job_manager_instance_id

    def get_host_id(self):
        """
        Determine the host key which matches the HOSTNAME string
        in this packet. If this HOSTNAME is not defined in the cache,
        attempt to insert it into the dns_cache table and return that id.

        Arguments:
        self -- A gram5packet.GRAM5Packet object
        cursor -- An SQL cursor to use if we need to insert this hostname into
        the table

        Returns:
        An integer key to the dns_cache table or None if the version is
        not defined or can't be parsed. As a side effect, this
        key may be newly defined and cached.

        """
        return GRAM5Packet.dns_cache.get_host_id(
                self.ip_address,
                self.data.get("HOSTNAME"))

    def get_lrm_id(self, cursor):
        lrm_id = None
        lrm = self.data.get("E")

        if lrm is not None:
            lrm_id = GRAM5Packet.__lrms.get(lrm)
            if lrm_id is None:
                values = (lrm,)
                cursor.execute('''
                    INSERT INTO gram5_lrms(lrm) VALUES(%s)
                    RETURNING id
                    ''', values)

                lrm_id = cursor.fetchone()[0]
                GRAM5Packet.__lrms[lrm] = lrm_id
        return lrm_id

    def get_version_id(self, cursor):
        """
        Determine the version id key which matches the version string
        in this packet. If the version is not defined in the cache,
        attempt to insert it into the gram5_versions table and return that id.

        Arguments:
        self -- A gram5packet.GRAM5Packet object
        cursor -- An SQL cursor to use if we need to insert this version into
        the table

        Returns:
        An integer key to the gram5_versions table or None if the version is
        not defined or can't be parsed. As a side effect, this
        key may be newly defined and cached.

        """
        version_id = None
        version = self.__parse_version()

        if version is not None:
            version_id = GRAM5Packet.__versions.get(version)
            if version_id is None:
                version_list = list(version)
                version_list[3] = GRAM5Packet.db_class.TimestampFromTicks(
                    version_list[3])
                values_sql = tuple(version_list)

                cursor.execute('''
                    INSERT INTO gram5_versions(
                        major,
                        minor,
                        flavor,
                        dirt_timestamp,
                        dirt_branch,
                        distro_string)
                    VALUES(%s, %s, %s, %s, %s, %s)
                    RETURNING id
                    ''', values_sql)
                version_id = cursor.fetchone()[0]
                GRAM5Packet.__versions[version] = version_id
        return version_id

    @staticmethod
    def __init_dns_cache(cursor):
        """
        Initialize the global DNSCache object which caches the values in
        the similarly-named table.

        Arguments:
        cursor -- An SQL cursor to use to read the dns_cache table

        Returns:
        None, but alters the class-wide variable dns_cache

        """
        GRAM5Packet.dns_cache = DNSCache(cursor)

    @staticmethod
    def __init_versions(cursor):
        """
        Initialize the global dictionary gftp_versions which caches the values in
        the similarly-named table.

        The gftp_versions dictionary maps
        (major, minor, flavor, dirt_timestamp, dirt_branch, distro_string) ->
            version_id

        Arguments:
        cursor -- An SQL cursor to use to read the gftp_versions table

        Returns:
        None, but alters the global variable gftp_versions.

        """
        cursor.execute("""
            SELECT  id,
                    major,
                    minor,
                    flavor,
                    dirt_timestamp,
                    dirt_branch,
                    distro_string
            FROM gram5_versions""")
        for row in cursor:
            [version_id, major, minor, flavor, dirt_timestamp, dirt_branch, \
                distro_string] = row
            dirt_timestamp = int(
                time.strftime("%s", dirt_timestamp.timetuple()))
            GRAM5Packet.__versions[
                (major, minor, flavor, dirt_timestamp, \
                dirt_branch, distro_string)] = version_id

    # Regular expression to handle GRAM5 Server version strings such as
    # 3.14 (gcc32dbg, 1222134484-78) [Globus Toolkit 4.2.0]
    version_re = re.compile(\
        "([0-9]+)\\.([0-9]+) \\(([^,]*), " + \
        "([0-9]+)-([0-9]+)\\)( \\[([^\\]]*)\\])?")

    def __parse_version(self):
        """
        Parse a gram version string of the form
        major.minor (flavor, dirttimestamp-dirtbranch) [distrostring]

        Arguments:
        verstring -- The string to parse

        Returns:
        A tuple containing (in order):
            major, minor, flavor, dirt_timestamp, dirt_branch, distro_string
        parsed from the verstring parameter.

        """
        verstring = self.data.get("D")
        if verstring == None:
            return None

        matches = GRAM5Packet.version_re.match(verstring)
        if matches != None:
            return (
                int(matches.group(1)),
                int(matches.group(2)),
                matches.group(3),
                int(matches.group(4)),
                int(matches.group(5)),
                matches.group(7))
        else:
            return None

    @staticmethod
    def __init_lrms(cursor):
        """
        Initialize the dictionary GRAM5Packet.__lrms which caches the values in
        the gram5_lrms table.

        The dictionary maps
        lrm -> id

        Arguments:
        cursor -- An SQL cursor to use to read the gram5_lrms table

        Returns:
        None, but alters the global variable GRAM5Packet.__lrms.

        """
        cursor.execute("""
            SELECT id, lrm
            FROM gram5_lrms""")
        for row in cursor:
            [lrm_id, lrm] = row
            GRAM5Packet.__lrms[lrm] = lrm_id

    @staticmethod
    def __init_job_managers(cursor):
        """
        Initialize the dictionary GRAM5Packet.__job_managers which caches the
        values in the gram5_job_managers table.

        The dictionary maps
        (host_id, version, lrm_id, seg_used, poll_used, audit_used) -> id

        Arguments:
        cursor -- An SQL cursor to use to read the gram5_job_managers table

        Returns:
        None, but alters the global variable GRAM5Packet.__job_managers.

        """

        cursor.execute("""
            SELECT id, host_id, version, lrm_id,
                    seg_used, poll_used, audit_used
            FROM gram5_job_managers""")
        for row in cursor:
            [jm_id, host_id, version, lrm_id, seg_used, poll_used, audit_used] \
                    = row
            values = (host_id, version, lrm_id, seg_used, poll_used, audit_used)
            GRAM5Packet.__job_managers[values] = jm_id

    @staticmethod
    def __init_job_manager_instances(cursor):
        """
        Initialize the dictionary GRAM5Packet.__job_manager_instances which
        caches the values in the gram5_job_manager_instances table.

        The dictionary maps
        (job_manager_id, uuid, start_time) -> id

        Arguments:
        cursor -- An SQL cursor to use to read the gram5_job_manager_instances
        table

        Returns:
        None, but alters the global variable
        GRAM5Packet.__job_manager_instances.

        """

        cursor.execute("""
            SELECT id, job_manager_id, uuid, start_time
            FROM gram5_job_manager_instances""")
        for row in cursor:
            [jmi_id, job_manager_id, uuid, start_time] = row
            values = (job_manager_id, uuid, start_time)
            GRAM5Packet.__job_manager_instances[values] = jmi_id
            GRAM5Packet.__job_manager_instances_by_uuid[uuid] = jmi_id

    @staticmethod
    def __init_rsl_attributes(cursor):
        """
        Initialize the dictionary GRAM5Packet.__rsl_attributes which
        caches the values in the gram5_rsl_attributes table.

        The dictionary maps
        (attribute_name) -> id

        Arguments:
        cursor -- An SQL cursor to use to read the gram5_rsl_attributes
        table

        Returns:
        None, but alters the global variable
        GRAM5Packet.__rsl_attributes.

        """

        cursor.execute("""
            SELECT id, attribute
            FROM gram5_rsl_attributes""")
        for row in cursor:
            [rsl_id, attribute] = row
            GRAM5Packet.__rsl_attributes[attribute] = rsl_id

    @staticmethod
    def __init_rsl_bitfields(cursor):
        """
        Initialize the dictionary GRAM5Packet.__rsl_bitfields which
        caches the values in the gram5_rsl_bitfields table.

        The dictionary maps
        (bitfield) -> bitfield

        Arguments:
        cursor -- An SQL cursor to use to read the gram5_rsl_attributes
        table

        Returns:
        None, but alters the global variable
        GRAM5Packet.__rsl_bitfields.

        """

        cursor.execute("""
            SELECT bitfield
            FROM gram5_rsl_attribute_groups""")
        for row in cursor:
            [bitfield] = row
            GRAM5Packet.__rsl_bitfields[bitfield] = bitfield

    @staticmethod
    def __init_job_type_ids(cursor):
        """
        Initialize the dictionary GRAM5Packet.job_type_ids which
        caches the values in the gram5_job_types table.

        The dictionary maps
        (jobtype) -> id

        Arguments:
        cursor -- An SQL cursor to use to read the gram5_job_types
        table

        Returns:
        None, but alters the global variable
        GRAM5Packet.job_type_ids.

        """

        cursor.execute("""
            SELECT id, jobtype
            FROM gram5_job_types""")
        for row in cursor:
            [id, jobtype] = row
            GRAM5Packet.job_type_ids[jobtype] = id

    @staticmethod
    def __init_clients(cursor):
        """
        Initialize the dictionary GRAM5Packet.clients which
        caches the values in the gram5_client table.

        The dictionary maps
        (host_id, dn) -> id

        Arguments:
        cursor -- An SQL cursor to use to read the gram5_client
        table

        Returns:
        None, but alters the global variable
        GRAM5Packet.clients.

        """

        cursor.execute("""
            SELECT id, host_id, dn
            FROM gram5_client""")
        for row in cursor:
            [id, host_id, dn] = row
            values = (host_id, dn)
            GRAM5Packet.clients[values] = id

    @staticmethod
    def __init_executables(cursor):
        """
        Initialize the dictionary GRAM5Packet.executables which
        caches the values in the gram5_executable table.

        The dictionary maps
        (host_id, dn) -> id

        Arguments:
        cursor -- An SQL cursor to use to read the gram5_executable
        table

        Returns:
        None, but alters the global variable
        GRAM5Packet.executables.

        """

        cursor.execute("""
            SELECT id, executable, arguments
            FROM gram5_executable""")
        for row in cursor:
            [id, executable, arguments] = row
            values = (id, executable, arguments)
            GRAM5Packet.executables[values] = id

    def get_lifetime(self):
        start_time = float(self.data.get('A'))
        send_time = self.send_time_ticks
        return "%f seconds" % (send_time - start_time)

    def get_rsl_attribute_index(self, attr, cursor):
        attribute_id = GRAM5Packet.__rsl_attributes.get(attr)
        if attribute_id is None:
            values = (attr, True)
            cursor.execute(
                '''INSERT INTO gram5_rsl_attributes(
                        attribute,
                        extension)
                    VALUES(%s, %s)
                    RETURNING id''',
                values)
            attribute_id = cursor.fetchone()[0]
            GRAM5Packet.__rsl_attributes[attr] = attribute_id
        return attribute_id

    def get_rsl_bitfield(self, cursor):
        # Job Manager sends standard attributes in a bitfield
        bitfield = int(self.data.get('1'))

        # Add bits to the bitfield for extension attributes
        attrs = self.data.get('4')
        if attrs is not None and attrs != '':
            extra_rsl = attrs.split(',')
            for attr in extra_rsl:
                attr_index = self.get_rsl_attribute_index(attr, cursor)
                bitfield = bitfield | (2**attr_index)

        attribute_list = []

        for (name, id) in GRAM5Packet.__rsl_attributes.items():
            if (bitfield & (2**int(id))) != 0:
                attribute_list.append(name)
        attribute_list.sort()

        if GRAM5Packet.__rsl_bitfields.get(bitfield) is None:
            cursor.execute('''
                    INSERT INTO gram5_rsl_attribute_groups(
                            bitfield,
                            attributes)
                    VALUES(%s, %s)''', (bitfield, ','.join(attribute_list)))
            GRAM5Packet.__rsl_bitfields[bitfield] = bitfield

        return bitfield

    def get_executable_id(self, cursor):
        executable_id = None
        executable = self.data.get('6')
        arguments = self.data.get('7')

        values = (executable, arguments)

        if executable is not None:
            executable_id = GRAM5Packet.executables.get(values)
            if executable_id is None:
                cursor.execute('''
                        INSERT INTO gram5_executable(
                            executable,
                            arguments)
                        VALUES(%s, %s)
                        RETURNING id''', values)
                executable_id = cursor.fetchone()[0]
                GRAM5Packet.executables[values] = executable_id
        return executable_id

class GRAM5JMPacket(GRAM5Packet):
    """
    GRAM5 Usage Packet handler for job manager status packets
    """
    
    def __init__(self, address, packet):
        GRAM5Packet.__init__(self, address, packet)

    
    insert_statement = '''
            INSERT INTO gram5_job_manager_status(
                job_manager_instance_id,
                restarted_jobs,
                status_time,
                lifetime,
                total_jobs,
                total_failed,
                total_canceled,
                total_done,
                total_dry_run,
                peak_jobs,
                current_jobs,
                unsubmitted,
                stage_in,
                pending,
                active,
                stage_out,
                failed,
                done)
            VALUES(
                %s, %s, %s, %s, %s, %s, %s, %s, %s, %s,
                %s, %s, %s, %s, %s, %s, %s, %s)'''

    def values(self, dbclass):
        """
        Return a values tuple which matches the parameters in the
        class's insert_statement.

        Arguments:
        self -- A GRAM5JMPacket object

        Returns:
        Tuple containing
            (job_manager_instance_id, restarted_jobs,
             status_time, lifetime, total_jobs, total_failed, total_canceled,
             total_done, total_dry_run, peak_jobs, current_jobs, unsubmitted,
             stage_in, pending, active, stage_out, failed, done)
        """

        values = (
            self.get_job_manager_instance_id(GRAM5Packet.cursor),
            self.data.get("I"),
            dbclass.Timestamp(*self.send_time),
            self.get_lifetime(),
            self.data.get("K"),
            self.data.get("L"),
            self.data.get("M"),
            self.data.get("N"),
            self.data.get("O"),
            self.data.get("P"),
            self.data.get("Q"),
            self.data.get("R"),
            self.data.get("S"),
            self.data.get("T"),
            self.data.get("U"),
            self.data.get("V"),
            self.data.get("W"),
            self.data.get("X"))
        return values

class GRAM5JobPacket(GRAM5Packet):
    """
    GRAM5 Usage Packet handler for job status packets
    """
    
    def __init__(self, address, packet):
        GRAM5Packet.__init__(self, address, packet)

    
    insert_statement = '''
            INSERT INTO gram5_job_status(
                job_id,
                send_time,
                unsubmitted_timestamp,
                file_stage_in_timestamp,
                pending_timestamp,
                active_timestamp,
                failed_timestamp,
                file_stage_out_timestamp,
                done_timestamp,
                status_count,
                register_count,
                unregister_count,
                signal_count,
                refresh_count,
                failure_code,
                restart_count,
                callback_count)
            VALUES(
                %s, %s, %s, %s, %s, %s, %s, %s, %s, %s,
                %s, %s, %s, %s, %s, %s, %s)'''

    def values(self, dbclass):
        """
        Return a values tuple which matches the parameters in the
        class's insert_statement.

        Arguments:
        self -- A GRAM5JobPacket object

        Returns:
        Tuple containing
            (job_id, send_time,
             unsubmitted_timestamp, file_stage_in_timestamp, pending_timestamp,
             active_timestamp, failed_timestamp, file_stage_out_timestamp,
             done_timestamp, status_count, register_count, unregister_count,
             signal_count, refresh_count, failure_code, restart_count,
             callback_count)
        """
        unsubmitted_timestamp = None
        file_stage_in_timestamp = None
        pending_timestamp = None
        active_timestamp = None
        failed_timestamp = None
        file_stage_out_timestamp = None
        done_timestamp = None

        if self.data.get('c') is not None and float(self.data.get('c')) > 1:
            unsubmitted_timestamp = GRAM5Packet.db_class.TimestampFromTicks(
                    float(self.data.get("c")))
        if self.data.get('d') is not None and float(self.data.get('d')) > 1:
            file_stage_in_timestamp = GRAM5Packet.db_class.TimestampFromTicks(
                    float(self.data.get("d")))
        if self.data.get('e') is not None and float(self.data.get('e')) > 1:
            pending_timestamp = GRAM5Packet.db_class.TimestampFromTicks(
                    float(self.data.get("e")))
        if self.data.get('f') is not None and float(self.data.get('f')) > 1:
            active_timestamp = GRAM5Packet.db_class.TimestampFromTicks(
                    float(self.data.get("f")))
        if self.data.get('g') is not None and float(self.data.get('g')) > 1:
             failed_timestamp = GRAM5Packet.db_class.TimestampFromTicks(
                    float(self.data.get("g")))
        if self.data.get('h') is not None and float(self.data.get('h')) > 1:
             file_stage_out_timestamp = GRAM5Packet.db_class.TimestampFromTicks(
                    float(self.data.get("h")))
        if self.data.get('i') is not None and float(self.data.get('i')) > 1:
             done_timestamp = GRAM5Packet.db_class.TimestampFromTicks(
                    float(self.data.get("i")))
        return (
            self.get_job_id(GRAM5Packet.cursor),
            dbclass.Timestamp(*self.send_time),
            unsubmitted_timestamp,
            file_stage_in_timestamp,
            pending_timestamp,
            active_timestamp,
            failed_timestamp,
            file_stage_out_timestamp,
            done_timestamp,
            self.data.get("k") or 0,
            self.data.get("l") or 0,
            self.data.get("2") or 0,
            self.data.get("m") or 0,
            self.data.get("n") or 0,
            self.data.get("j") or 0,
            self.data.get("Y") or 0,
            self.data.get("Z") or 0)

    def get_job_id(self, cursor):
        job_id = None
        job_manager_id = self.get_job_manager_instance_id_by_uuid(cursor)
        count = self.data.get("3")
        if count is None:
            count = 0

        host_count = self.data.get("b")
        if host_count is None:
            host_count = 0
        dryrun = self.data.get('a') == '1'
        client_id = self.get_client_id(cursor)
        executable_id = self.get_executable_id(cursor)
        rsl_bitfield = self.get_rsl_bitfield(cursor)
        gram5_job_file_info = self.get_file_info(cursor)
        jobtype = self.get_job_type(cursor);

        values = (
            job_manager_id,
            GRAM5Packet.db_class.Timestamp(*self.send_time),
            count,
            host_count,
            dryrun,
            client_id,
            executable_id,
            rsl_bitfield,
            jobtype,
            gram5_job_file_info)

        cursor.execute('''
                INSERT INTO gram5_jobs(
                    job_manager_id,
                    send_time,
                    count,
                    host_count,
                    dryrun,
                    client_id,
                    executable_id,
                    rsl_bitfield,
                    jobtype,
                    gram5_job_file_info)
                VALUES(%s, %s, %s, %s, %s, %s, %s, %s, %s, %s)
                RETURNING id''', values)
        job_id = cursor.fetchone()[0]
        return job_id

    def get_client_id(self, cursor):
        host_id = None
        client_id = None

        client_ip = self.data.get('8')
        user_dn = self.data.get('9')

        if client_ip is not None:
            [address, port] = client_ip.split(':')
            host_id = GRAM5Packet.dns_cache.get_host_id(address)

        values = (host_id, user_dn)

        if host_id is not None or user_dn is not None:
            client_id = GRAM5Packet.clients.get(values)

            if client_id is None:
                cursor.execute('''
                        INSERT INTO gram5_client(
                            host_id,
                            dn)
                        VALUES(%s, %s)
                        RETURNING id''', values)
                client_id = cursor.fetchone()[0]
                GRAM5Packet.clients[values] = client_id
        return client_id

    def get_file_info(self, cursor):
        file_info_id = None

        file_clean_up = self.data.get('o')

        file_stage_in_http = self.data.get('p')
        file_stage_in_https = self.data.get('q')
        file_stage_in_ftp = self.data.get('r')
        file_stage_in_gsiftp = self.data.get('s')

        file_stage_in_shared_http = self.data.get('t')
        file_stage_in_shared_https = self.data.get('u')
        file_stage_in_shared_ftp = self.data.get('v')
        file_stage_in_shared_gsiftp = self.data.get('w')

        file_stage_out_http = self.data.get('x')
        file_stage_out_https = self.data.get('y')
        file_stage_out_ftp = self.data.get('z')
        file_stage_out_gsiftp = self.data.get('0')

        if file_clean_up is None:
            file_clean_up = 0

        if file_stage_in_http is None:
            file_stage_in_http = 0

        if file_stage_in_https is None:
            file_stage_in_https = 0

        if file_stage_in_ftp is None:
            file_stage_in_ftp = 0

        if file_stage_in_gsiftp is None:
            file_stage_in_gsiftp = 0

        if file_stage_in_shared_http is None:
            file_stage_in_shared_http = 0

        if file_stage_in_shared_https is None:
            file_stage_in_shared_https = 0

        if file_stage_in_shared_ftp is None:
            file_stage_in_shared_ftp = 0

        if file_stage_in_shared_gsiftp is None:
            file_stage_in_shared_gsiftp = 0

        if file_stage_out_http is None:
            file_stage_out_http = 0

        if file_stage_out_https is None:
            file_stage_out_https = 0

        if file_stage_out_ftp is None:
            file_stage_out_ftp = 0
        
        if file_stage_out_gsiftp is None:
            file_stage_out_gsiftp = 0

        values = (
            file_clean_up,
            file_stage_in_http,
            file_stage_in_https,
            file_stage_in_ftp,
            file_stage_in_gsiftp,
            file_stage_in_shared_http,
            file_stage_in_shared_https,
            file_stage_in_shared_ftp,
            file_stage_in_shared_gsiftp,
            file_stage_out_http,
            file_stage_out_https,
            file_stage_out_ftp,
            file_stage_out_gsiftp)

        if file_clean_up != 0 or \
                file_stage_in_http != 0 or \
                file_stage_in_https != 0 or \
                file_stage_in_ftp != 0 or \
                file_stage_in_gsiftp != 0 or \
                file_stage_in_shared_http != 0 or \
                file_stage_in_shared_https != 0 or \
                file_stage_in_shared_ftp != 0 or \
                file_stage_in_shared_gsiftp != 0 or \
                file_stage_out_http != 0 or \
                file_stage_out_https != 0 or \
                file_stage_out_ftp != 0 or \
                file_stage_out_gsiftp != 0:
            cursor.execute('''
                INSERT into gram5_job_file_info(
                    file_clean_up,
                    file_stage_in_http,
                    file_stage_in_https,
                    file_stage_in_ftp,
                    file_stage_in_gsiftp,
                    file_stage_in_shared_http,
                    file_stage_in_shared_https,
                    file_stage_in_shared_ftp,
                    file_stage_in_shared_gsiftp,
                    file_stage_out_http,
                    file_stage_out_https,
                    file_stage_out_ftp,
                    file_stage_out_gsiftp)
                VALUES(%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s)
                RETURNING id''',
                values)
            file_info_id = cursor.fetchone()[0]
        return file_info_id

    def get_job_type(self, cursor):
        job_type_id = None

        job_type = self.data.get('H')

        values = (job_type,)

        job_type_id = GRAM5Packet.job_type_ids.get(job_type);

        if job_type_id == None:
            cursor.execute('''
                INSERT into gram5_job_types(
                    jobtype)
                VALUES(%s)
                RETURNING id''',
                values)
            job_type_id = cursor.fetchone()[0]
            GRAM5Packet.job_type_ids[job_type] = job_type_id
        return job_type_id
# vim: ts=4:sw=4:syntax=python
