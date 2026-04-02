package com.robotcontroller;

import android.os.Bundle;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;
import androidx.appcompat.app.AppCompatActivity;

/**
 * Generic camera-only activity.
 * Used by Noise Mode and Guide Wire Mode (after path selection).
 *
 * Intent extras:
 *   MODE_TITLE  - label shown top-right (e.g., "MAZE MODE")
 *   SUBTITLE    - optional label shown below title (e.g., "Path 1")
 */
public class CameraViewActivity extends AppCompatActivity {

    private MjpegStream mjpegStream;
    private String modeTitle;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_camera_view);

        ImageView cameraView = findViewById(R.id.camera_view);
        TextView tvStreamStatus = findViewById(R.id.tv_stream_status);
        TextView tvModeTitle = findViewById(R.id.tv_mode_title);
        TextView tvSubtitle = findViewById(R.id.tv_subtitle);

        // Back button — send stop and exit
        findViewById(R.id.btn_back).setOnClickListener(v -> {
            BluetoothService.getInstance().sendStop();
            finish();
        });

        // Set title and subtitle from intent
        modeTitle = getIntent().getStringExtra("MODE_TITLE");
        String subtitle = getIntent().getStringExtra("SUBTITLE");

        if (modeTitle != null) {
            tvModeTitle.setText(modeTitle);
        }
        if (subtitle != null && !subtitle.isEmpty()) {
            tvSubtitle.setText(subtitle);
            tvSubtitle.setVisibility(View.VISIBLE);
        } else {
            tvSubtitle.setVisibility(View.GONE);
        }

        // MJPEG stream
        mjpegStream = new MjpegStream(cameraView);
        mjpegStream.setStatusListener(tvStreamStatus::setText);
        tvStreamStatus.setText("Connecting to camera...");
    }

    @Override
    protected void onResume() {
        super.onResume();
        // Send mode enter command for autonomous modes
        if (modeTitle != null) {
            switch (modeTitle) {
                case "NOISE":
                    BluetoothService.getInstance().sendEnterNoise();
                    break;
                // GUIDE WIRE: path command already sent by GuideWirePathActivity
            }
        }
        mjpegStream.start();
    }

    @Override
    protected void onPause() {
        super.onPause();
        mjpegStream.stop();
        BluetoothService.getInstance().sendStop();
    }
}
