#ifndef GLOBUS_I_GFS_CONFIG_H
#define GLOBUS_I_GFS_CONFIG_H

#define globus_i_gfs_config_list    (globus_list_t *) globus_i_gfs_config_get
#define globus_i_gfs_config_int     (int) globus_i_gfs_config_get
#define globus_i_gfs_config_string  (char *) globus_i_gfs_config_get
#define globus_i_gfs_config_bool    (globus_bool_t) globus_i_gfs_config_get

void
globus_i_gfs_config_init(
    int                                 argc,
    char **                             argv);

void *
globus_i_gfs_config_get(
    const char *                        option_name);
    
#endif
