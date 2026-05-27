package com.mplayer;

import android.app.Activity;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.widget.Button;
import android.widget.EditText;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.Toast;

public class MPlayerActivity extends Activity implements SurfaceHolder.Callback {

    private MPlayerNative player;
    private SurfaceView surfaceView;
    private EditText urlInput;
    private SeekBar seekBar;
    private SeekBar volumeBar;
    private Button btnPlay;
    private Button btnStop;
    private Button btnOpen;
    private TextView tvInfo;
    private Handler handler;
    private boolean isTrackingSeekBar = false;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_player);

        player = new MPlayerNative();
        handler = new Handler(Looper.getMainLooper());

        surfaceView = findViewById(R.id.surfaceView);
        surfaceView.getHolder().addCallback(this);

        urlInput = findViewById(R.id.urlInput);
        btnOpen = findViewById(R.id.btnOpen);
        btnPlay = findViewById(R.id.btnPlay);
        btnStop = findViewById(R.id.btnStop);
        seekBar = findViewById(R.id.seekBar);
        volumeBar = findViewById(R.id.volumeBar);
        tvInfo = findViewById(R.id.tvInfo);

        btnOpen.setOnClickListener(v -> {
            String url = urlInput.getText().toString().trim();
            if (url.isEmpty()) {
                Toast.makeText(this, "Please enter a URL or file path", Toast.LENGTH_SHORT).show();
                return;
            }
            player.stop();
            player.open(url);
            player.play();
        });

        btnPlay.setOnClickListener(v -> {
            if (player.getCurrentPosition() > 0) {
                player.pause();
                btnPlay.setText("Play");
            } else {
                player.play();
                btnPlay.setText("Pause");
            }
        });

        btnStop.setOnClickListener(v -> {
            player.stop();
            btnPlay.setText("Play");
        });

        seekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                if (fromUser) {
                    double duration = player.getDuration();
                    player.seek(progress / 100.0 * duration);
                }
            }
            @Override
            public void onStartTrackingTouch(SeekBar seekBar) { isTrackingSeekBar = true; }
            @Override
            public void onStopTrackingTouch(SeekBar seekBar) { isTrackingSeekBar = false; }
        });

        volumeBar.setProgress(80);
        volumeBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                if (fromUser) player.setVolume(progress / 100.0f);
            }
            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {}
            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {}
        });

        startProgressUpdate();
    }

    private void startProgressUpdate() {
        handler.postDelayed(new Runnable() {
            @Override
            public void run() {
                if (!isTrackingSeekBar) {
                    double pos = player.getCurrentPosition();
                    double dur = player.getDuration();
                    if (dur > 0) {
                        seekBar.setProgress((int)(pos / dur * 100));
                    }
                }
                handler.postDelayed(this, 200);
            }
        }, 200);
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        player.setSurface(holder.getSurface());
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {}

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        player.stop();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        handler.removeCallbacksAndMessages(null);
        player.release();
    }
}
