Import("env")
import datetime

version = str(datetime.date.today().year)[2:4] + "." + str(datetime.date.today().month) + "." + str(datetime.date.today().day) + "_" + datetime.datetime.now().strftime('%H%M')
env['BUILD_FLAGS'][0] = "\'-D VERSION=\"" + version + "\"\'"
print (env['BUILD_FLAGS'])

my_flags = env.ParseFlags(env['BUILD_FLAGS'])
defines = {k: v for (k, v) in my_flags.get("CPPDEFINES")}
env.Replace(PROGNAME="%s" % defines.get(" VERSION")[1:-1])
