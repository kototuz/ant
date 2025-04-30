#include <jni.h>
#include <pty.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#include <android/log.h>

#define LOG(prio, fmt, ...) __android_log_print(ANDROID_LOG_##prio, "ant", fmt, __VA_ARGS__)

void prepare_shell(void)
{
    // We can run programs (using `linker64`) only in app folders
    if (mkdir("/data/data/com.kototuz.ant/home", 0777) == -1 && errno != EEXIST) {
        LOG(ERROR, "ERROR: Could not create home directory: %s", strerror(errno));
        return;
    }
    if (setenv("HOME", "/data/data/com.kototuz.ant/home", 1) == -1) {
        LOG(ERROR, "ERROR: Could not set 'HOME': %s", strerror(errno));
    }

    // Create run script to run executables by `$r <program>`
    // NOTE: It is not work with scripts
    FILE *file = fopen("/data/data/com.kototuz.ant/home/.run.sh", "w");
    if (file == NULL) {
        LOG(ERROR, "ERROR: Could not open run script: %s", strerror(errno));
        return;
    }
    fputs("linker64 `pwd`/$@\n", file);
    if (setenv("r", "sh /data/data/com.kototuz.ant/home/.run.sh", 1) == -1) {
        LOG(ERROR, "ERROR: Could not set 'r': %s", strerror(errno));
    }
    fclose(file);
}

JNIEXPORT jint JNICALL Java_com_kototuz_ant_NativeLoader_spawnShell(JNIEnv *env, jobject this)
{
    int fd;
    pid_t pid = forkpty(&fd, NULL, NULL, NULL);
    if (pid == -1) return -1;

    if (pid == 0) {
        prepare_shell();
        char prog[] = "sh";
        char *args[] = {&prog[0], NULL};
        int status = execvp("sh", &args[0]);
        LOG(ERROR, "ERROR: Could not spawn shell: %s", strerror(status));
        exit(1);
    } else {
        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        return fd;
    }
}

JNIEXPORT jbyteArray JNICALL Java_com_kototuz_ant_NativeLoader_readShell(JNIEnv *env, jobject this, jint pty_fd)
{
    jbyte buf[4096] = {0};

    jbyteArray result;
    ssize_t read_len = read(pty_fd, buf, sizeof(buf));
    if (read_len == -1) {
        result = (*env)->NewByteArray(env, 0);
        if (errno != EAGAIN) {
            LOG(ERROR, "ERROR: Could not read shell: %s", strerror(errno));
        }
    } else {
        result = (*env)->NewByteArray(env, read_len);
        (*env)->SetByteArrayRegion(env, result, 0, read_len, buf);
    }


    return result;
}

JNIEXPORT void JNICALL Java_com_kototuz_ant_NativeLoader_writeShell(JNIEnv *env, jobject this, jint pty_fd, jbyteArray byte_arr)
{
    jbyte *bytes = (*env)->GetByteArrayElements(env, byte_arr, NULL);
    jsize len = (*env)->GetArrayLength(env, byte_arr);

    jbyte *ptr = bytes;
    while (len > 0) {
        ssize_t written = write(pty_fd, ptr, len);
        if (written == -1) {
            LOG(ERROR, "ERROR: Could not write shell: %s", strerror(errno));
            len = 0;
        } else {
            len -= written;
            ptr += written;
        }
    }

    (*env)->ReleaseByteArrayElements(env, byte_arr, bytes, 0);
}
