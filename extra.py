Import("env")

my_flags = env.ParseFlags(env['BUILD_FLAGS'])
defines = {k: v for (k, v) in my_flags.get("CPPDEFINES")}

print(defines)
print(defines.get(" VERSION"))
env.Replace(PROGNAME="%s" % defines.get(" VERSION")[1:-1])