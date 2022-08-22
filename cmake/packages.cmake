set(PROXY_VERSION "0.0.1" CACHE STRING "version of the proxy library")

CPMAddPackage(
    NAME proxy
    VERSION ${PROXY_VERSION}
    GIT_REPOSITORY https://gitlab.com/blrevive/tools/proxy
    GIT_TAG v1.0.0-beta.2
)
