#if !defined GLOBUS_XIO_TOKEN_BUCKET_DRIVER_H
#define GLOBUS_XIO_TOKEN_BUCKET_DRIVER_H 1

enum
{
    GLOBUS_XIO_TOKEN_BUCKET_SET_RATE = 1,
    GLOBUS_XIO_TOKEN_BUCKET_SET_PERIOD,
    GLOBUS_XIO_TOKEN_BUCKET_SET_READ_RATE,
    GLOBUS_XIO_TOKEN_BUCKET_SET_READ_PERIOD,
    GLOBUS_XIO_TOKEN_BUCKET_SET_WRITE_RATE,
    GLOBUS_XIO_TOKEN_BUCKET_SET_WRITE_PERIOD,
    GLOBUS_XIO_TOKEN_BUCKET_SET_BURST,
    GLOBUS_XIO_TOKEN_BUCKET_SET_READ_BURST,
    GLOBUS_XIO_TOKEN_BUCKET_SET_WRITE_BURST,
    GLOBUS_XIO_TOKEN_BUCKET_SET_GROUP,
    GLOBUS_XIO_TOKEN_BUCKET_SET_READ_GROUP,
    GLOBUS_XIO_TOKEN_BUCKET_SET_WRITE_GROUP
};

globus_result_t
globus_xio_token_bucket_set_read_group(
    char *                              group_name,
    globus_off_t                        rate,
    int                                 us_period,
    globus_size_t                       burst_size,
    globus_bool_t *                     in_out_create);

globus_result_t
globus_xio_token_bucket_set_write_group(
    char *                              group_name,
    globus_off_t                        rate,
    int                                 us_period,
    globus_size_t                       burst_size,
    globus_bool_t *                     in_out_create);


#endif
