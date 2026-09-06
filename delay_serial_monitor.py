Import("env")

def after_upload(source, target, env):
    print("Wait for device/port to get online again...")
    import time
    time.sleep(2)

env.AddPostAction("upload", after_upload)