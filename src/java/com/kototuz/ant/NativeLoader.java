package com.kototuz.ant;

import android.os.Bundle;
import android.os.Handler;
import android.content.Context;
import android.util.Log;
import android.widget.TextView;
import android.widget.EditText;
import android.view.inputmethod.*;
import android.view.*;
import android.text.Layout;
import android.text.method.ScrollingMovementMethod;
import android.graphics.Color;

import java.nio.charset.StandardCharsets;
import java.io.*;

public class NativeLoader extends android.app.Activity {
    private TextView output;
    private EditText prompt;
    private int pty_fd;
    private String shellBuffer;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        System.loadLibrary("main");
        setContentView(R.layout.simple_layout);

        output = findViewById(R.id.output);
        output.setMovementMethod(new ScrollingMovementMethod());
        output.setTextColor(Color.WHITE);

        prompt = findViewById(R.id.prompt);
        prompt.setOnEditorActionListener(new TextView.OnEditorActionListener() {
            @Override
            public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
                boolean handled = false;
                if (actionId == EditorInfo.IME_ACTION_SEND) {
                    String prompt_text = prompt.getText().toString() + "\n";
                    writeShell(pty_fd, prompt_text.getBytes());
                    prompt.setText("");
                    handled = true;
                }
                return handled;
            }
        });
        prompt.requestFocus();

        pty_fd = spawnShell();
        shellBuffer = new String();

        Handler handler = new Handler();
        handler.post(new Runnable(){ 
            @Override
            public void run() {
                byte[] bytes = readShell(pty_fd);
                if (bytes.length > 0) {
                    String str = new String(bytes, StandardCharsets.UTF_8);
                    if (str.startsWith("\033[2J\033[H")) {
                        shellBuffer = str.substring(7, str.length());
                    } else {
                        shellBuffer += str;
                    }
                    output.setText(shellBuffer);
                }

                handler.postDelayed(this, 0);
            }
        });

    }

    private native int spawnShell();
    private native byte[] readShell(int pty_fd);
    private native void writeShell(int pty_fd, byte[] bytes);
}
