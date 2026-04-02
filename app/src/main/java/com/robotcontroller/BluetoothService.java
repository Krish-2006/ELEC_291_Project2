package com.robotcontroller;

import android.Manifest;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothSocket;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Build;
import android.util.Log;

import androidx.core.content.ContextCompat;

import java.io.IOException;
import java.io.OutputStream;
import java.util.Set;
import java.util.UUID;

/**
 * Singleton Bluetooth service for HC-06 communication.
 *
 * Finds the HC-06 by name from paired devices and maintains
 * a persistent serial connection. Sends single-character commands:
 *
 * Joystick / Motor:
 *   'F' = Forward
 *   'B' = Backward
 *   'L' = Left
 *   'R' = Right
 *   'S' = Stop / exit current mode
 *
 * Mode entry:
 *   'm' = Manual mode
 *   'N' = Noise mode
 *   'E' = Learning mode
 *
 * Guidewire paths:
 *   '1' = Guidewire path 1
 *   '2' = Guidewire path 2
 *   '3' = Guidewire path 3
 *
 * Learning sub-commands:
 *   'T' = Start recording (joystick replay)
 *   'U' = Start recording (sensor replay)
 *   'P' = Replay recorded path
 */
public class BluetoothService {

    private static final String TAG = "BluetoothService";

    // HC-06 default name. Change this if you've renamed your module.
    private static final String HC06_NAME = "HC-06";

    // Standard SPP UUID for Bluetooth Classic serial
    private static final UUID SPP_UUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB");

    // Singleton instance
    private static BluetoothService instance;

    private BluetoothAdapter bluetoothAdapter;
    private BluetoothSocket socket;
    private OutputStream outputStream;
    private boolean isConnected = false;

    private ConnectionListener connectionListener;

    public interface ConnectionListener {
        void onConnectionUpdate(String status, boolean connected);
    }

    private BluetoothService() {
        bluetoothAdapter = BluetoothAdapter.getDefaultAdapter();
    }

    public static synchronized BluetoothService getInstance() {
        if (instance == null) {
            instance = new BluetoothService();
        }
        return instance;
    }

    public void setConnectionListener(ConnectionListener listener) {
        this.connectionListener = listener;
    }

    public boolean isConnected() {
        return isConnected;
    }

    /**
     * Connect to the HC-06 module.
     * Must be called from a background thread.
     */
    public void connect(Context context) {
        if (isConnected) {
            notifyStatus("Already connected", true);
            return;
        }

        if (bluetoothAdapter == null) {
            notifyStatus("Bluetooth not supported", false);
            return;
        }

        if (!bluetoothAdapter.isEnabled()) {
            notifyStatus("Bluetooth is off — turn it on in Settings", false);
            return;
        }

        // Check permission
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            if (ContextCompat.checkSelfPermission(context, Manifest.permission.BLUETOOTH_CONNECT)
                    != PackageManager.PERMISSION_GRANTED) {
                notifyStatus("Bluetooth permission not granted", false);
                return;
            }
        }

        // Find HC-06 in paired devices
        BluetoothDevice hc06 = findHC06(context);
        if (hc06 == null) {
            notifyStatus("HC-06 not found — pair it in Settings first", false);
            return;
        }

        // Connect
        try {
            notifyStatus("Connecting to HC-06...", false);

            socket = hc06.createRfcommSocketToServiceRecord(SPP_UUID);
            bluetoothAdapter.cancelDiscovery(); // Cancel discovery to speed up connection
            socket.connect();

            outputStream = socket.getOutputStream();
            isConnected = true;

            notifyStatus("Connected to HC-06", true);
            Log.i(TAG, "Connected to HC-06 successfully");

        } catch (IOException e) {
            Log.e(TAG, "Connection failed: " + e.getMessage());
            notifyStatus("Connection failed — is HC-06 powered on?", false);
            disconnect();
        } catch (SecurityException e) {
            Log.e(TAG, "Permission error: " + e.getMessage());
            notifyStatus("Bluetooth permission denied", false);
        }
    }

    /**
     * Disconnect from HC-06.
     */
    public void disconnect() {
        isConnected = false;
        try {
            if (outputStream != null) outputStream.close();
        } catch (IOException ignored) {}
        try {
            if (socket != null) socket.close();
        } catch (IOException ignored) {}
        outputStream = null;
        socket = null;
    }

    /**
     * Send a single-character command to the HC-06.
     * Safe to call from any thread.
     */
    public void send(char command) {
        if (!isConnected || outputStream == null) {
            Log.w(TAG, "Not connected, can't send: " + command);
            return;
        }

        new Thread(() -> {
            try {
                outputStream.write(command);
                outputStream.flush();
                Log.d(TAG, "Sent: " + command);
            } catch (IOException e) {
                Log.e(TAG, "Send failed: " + e.getMessage());
                isConnected = false;
                notifyStatus("Connection lost", false);
            }
        }).start();
    }

    // Joystick / Motor
    public void sendForward()  { send('F'); }
    public void sendBackward() { send('B'); }
    public void sendLeft()     { send('L'); }
    public void sendRight()    { send('R'); }
    public void sendStop()     { send('S'); }

    // Mode entry
    public void sendEnterManual()   { send('m'); }
    public void sendEnterNoise()    { send('N'); }
    public void sendEnterLearning() { send('E'); }

    // Guidewire paths
    public void sendGuidewirePath(int path) { send((char)('0' + path)); }

    // Learning sub-commands
    public void sendStartJoystickRecord() { send('T'); }
    public void sendStartSensorRecord()   { send('U'); }
    public void sendReplay()              { send('P'); }

    /**
     * Find the HC-06 device from the list of paired devices.
     */
    private BluetoothDevice findHC06(Context context) {
        try {
            Set<BluetoothDevice> pairedDevices = bluetoothAdapter.getBondedDevices();
            if (pairedDevices == null) return null;

            for (BluetoothDevice device : pairedDevices) {
                String name = device.getName();
                if (name != null && name.equals(HC06_NAME)) {
                    Log.i(TAG, "Found HC-06: " + device.getAddress());
                    return device;
                }
            }
        } catch (SecurityException e) {
            Log.e(TAG, "Permission error finding HC-06: " + e.getMessage());
        }

        return null;
    }

    private void notifyStatus(String status, boolean connected) {
        Log.i(TAG, "Status: " + status + " (connected=" + connected + ")");
        if (connectionListener != null) {
            connectionListener.onConnectionUpdate(status, connected);
        }
    }
}
