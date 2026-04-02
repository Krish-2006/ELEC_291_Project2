package com.robotcontroller;

import android.os.Bundle;
import android.widget.ImageView;
import android.widget.TextView;
import androidx.appcompat.app.AppCompatActivity;

public class ManualModeActivity extends AppCompatActivity {

    private MjpegStream mjpegStream;
    private char lastCommand = 'S'; // Track last sent command to avoid spamming

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_manual_mode);

        ImageView cameraView = findViewById(R.id.camera_view);
        JoystickView joystickView = findViewById(R.id.joystick_view);
        TextView tvJoystickInfo = findViewById(R.id.tv_joystick_info);
        TextView tvStreamStatus = findViewById(R.id.tv_stream_status);

        // Back button
        findViewById(R.id.btn_back).setOnClickListener(v -> finish());

        // MJPEG stream
        mjpegStream = new MjpegStream(cameraView);
        mjpegStream.setStatusListener(tvStreamStatus::setText);
        tvStreamStatus.setText("Connecting to camera...");

        // Joystick listener — sends Bluetooth commands
        joystickView.setOnJoystickMoveListener((xPercent, yPercent, angle, strength) -> {
            String info = String.format("X: %d%%  Y: %d%%", xPercent, yPercent);
            tvJoystickInfo.setText(info);

            // Determine direction command
            char cmd = joystickToCommand(xPercent, yPercent, strength);

            // Only send if command changed (avoid flooding)
            if (cmd != lastCommand) {
                lastCommand = cmd;
                BluetoothService.getInstance().send(cmd);
            }
        });
    }

    /**
     * Convert joystick position to a single direction command.
     * Only one direction at a time — picks the dominant axis.
     */
    private char joystickToCommand(int xPercent, int yPercent, int strength) {
        // Dead zone — joystick near center
        if (strength < 25) {
            return 'S'; // Stop
        }

        // Determine dominant direction
        int absX = Math.abs(xPercent);
        int absY = Math.abs(yPercent);

        if (absY >= absX) {
            // Vertical dominant
            if (yPercent < 0) {
                return 'F'; // Forward (up on screen = negative Y)
            } else {
                return 'B'; // Backward
            }
        } else {
            // Horizontal dominant
            if (xPercent > 0) {
                return 'R'; // Right
            } else {
                return 'L'; // Left
            }
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        BluetoothService.getInstance().sendEnterManual();
        mjpegStream.start();
    }

    @Override
    protected void onPause() {
        super.onPause();
        mjpegStream.stop();
        // Stop motors and exit manual mode on STM32
        BluetoothService.getInstance().sendStop();
        lastCommand = 'S';
    }
}
