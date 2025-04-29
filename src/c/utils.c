#include <jni.h>
#include <pty.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#include <android/log.h>

#define TAG "ant"

JNIEXPORT jint JNICALL Java_com_kototuz_ant_NativeLoader_spawnShell(JNIEnv *env, jobject this)
{
    int fd;
    pid_t pid = forkpty(&fd, NULL, NULL, NULL);
    if (pid == -1) return -1;

    if (pid == 0) {
        if (mkdir("/data/data/com.kototuz.ant/home", 0777) == -1) {
            __android_log_print(ANDROID_LOG_ERROR, TAG, "ERROR: Could not create home directory: %s", strerror(errno));
        }
        if (setenv("HOME", "/data/data/com.kototuz.ant/home", 1) == -1) {
            __android_log_print(ANDROID_LOG_ERROR, TAG, "ERROR: Could not set 'HOME': %s", strerror(errno));
        }

        char prog[] = "sh";
        char *args[] = {&prog[0], NULL};
        int status = execvp("sh", &args[0]);
        __android_log_print(ANDROID_LOG_ERROR, TAG, "ERROR: Could not spawn shell: %s", strerror(status));
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
            __android_log_print(ANDROID_LOG_ERROR, TAG, "ERROR: Could not read shell: %s", strerror(errno));
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
            __android_log_print(ANDROID_LOG_ERROR, TAG, "ERROR: Could not write shell: %s", strerror(errno));
            len = 0;
        } else {
            len -= written;
            ptr += written;
        }
    }

    (*env)->ReleaseByteArrayElements(env, byte_arr, bytes, 0);
}
