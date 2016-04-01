PROGRAM(urlbench)

CFLAGS(
        -std=gnu++0x
)

PEERDIR(
    library/gettimeofday
    library/gurl
)

SRCS(
    main.cpp
)

END()
