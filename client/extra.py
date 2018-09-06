Import("env")

build_tag = "v1.50.0"

env.Replace(PROGNAME="%s" % build_tag)