/**
 * @file config.h
 * @brief Global project configuration and metadata for DUPLICATE-X.
 * @author DataVault Team (Academic Year 2026-2027)
 */

#ifndef DATAVAULT_CONFIG_H
#define DATAVAULT_CONFIG_H

/* Project Branding & Metadata */
#define PROJECT_NAME        "DUPLICATE-X"
#define TEAM_NAME           "DataVault"
#define PROJECT_TAGLINE     "Smarter Storage. Zero Duplicate Waste."
#define PROJECT_VERSION     "1.0.0-dev"
#define ACADEMIC_YEAR       "2026-2027"

/* System Buffer & Pipeline Limits */
#define BUFFER_SIZE_64K     65536       /* 64 KB buffer for streaming I/O */
#define CHUNK_SAMPLE_SIZE   4096        /* 4 KB head/tail sample chunk */
#define MAX_PATH_LENGTH     1024        /* Max characters in a filesystem path */
#define HASH_HEX_LENGTH     65          /* 64 hex chars for SHA-256 + null */

/* Default Network Configuration */
#define DEFAULT_SERVER_PORT 8080
#define DEFAULT_SERVER_HOST "127.0.0.1"

#endif /* DATAVAULT_CONFIG_H */

