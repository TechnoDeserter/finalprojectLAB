import 'dart:async';
import 'dart:convert';
import 'package:flutter/material.dart';
import 'package:http/http.dart' as http;
import 'package:shared_preferences/shared_preferences.dart';

class ESP32Service with ChangeNotifier {
  String? _esp32IP;
  bool _isConnected = false;
  bool _isReconnecting = false;
  String? _ssid;
  List<bool> _relayStates = [false, false, false, false];
  List<TimeOfDay> _onTimes = [
    const TimeOfDay(hour: 8, minute: 0),
    const TimeOfDay(hour: 9, minute: 0),
    const TimeOfDay(hour: 10, minute: 0),
    const TimeOfDay(hour: 11, minute: 0),
  ];
  List<TimeOfDay> _offTimes = [
    const TimeOfDay(hour: 18, minute: 0),
    const TimeOfDay(hour: 19, minute: 0),
    const TimeOfDay(hour: 20, minute: 0),
    const TimeOfDay(hour: 21, minute: 0),
  ];
  Timer? _pollingTimer;

  String? get esp32IP => _esp32IP;
  bool get isConnected => _isConnected;
  bool get isReconnecting => _isReconnecting;
  String? get ssid => _ssid;
  List<bool> get relayStates => _relayStates;
  List<TimeOfDay> get onTimes => _onTimes;
  List<TimeOfDay> get offTimes => _offTimes;

  ESP32Service() {
    _loadSavedIP();
  }

  Future<void> _loadSavedIP() async {
    final prefs = await SharedPreferences.getInstance();
    _esp32IP = prefs.getString('esp32IP');
    _ssid = prefs.getString('lastSSID');
    if (_esp32IP != null) {
      await tryReconnect();
      _startPolling();
    }
    notifyListeners();
  }

  Future<void> setESP32IP(String ip) async {
    _esp32IP = ip;
    _isConnected = true;
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString('esp32IP', ip);
    _startPolling();
    notifyListeners();
  }

  Future<void> tryReconnect() async {
    if (_esp32IP == null) return;
    _isReconnecting = true;
    notifyListeners();
    try {
      final response = await http
          .get(Uri.parse('http://$_esp32IP/'))
          .timeout(const Duration(seconds: 5));
      if (response.statusCode == 200) {
        final data = jsonDecode(response.body);
        if (data['success'] == true) {
          _isConnected = true;
          _ssid = data['ssid'];
          await _fetchStatus();
        } else {
          _isConnected = false;
        }
      } else {
        _isConnected = false;
      }
    } catch (e) {
      _isConnected = false;
    }
    _isReconnecting = false;
    notifyListeners();
  }

  Future<void> _fetchStatus() async {
    if (_esp32IP == null || !_isConnected) return;
    try {
      final response = await http
          .get(Uri.parse('http://$_esp32IP/getStatus'))
          .timeout(const Duration(seconds: 5));
      if (response.statusCode == 200) {
        final data = jsonDecode(response.body);
        if (data['success'] == true) {
          List<dynamic> relays = data['relays'];
          for (var relay in relays) {
            int index = relay['relay'] - 1;
            _relayStates[index] = relay['state'] == 1;
            _onTimes[index] = _parseTime(relay['onTime']);
            _offTimes[index] = _parseTime(relay['offTime']);
          }
          notifyListeners();
        }
      }
    } catch (e) {
      print('Error fetching status: $e');
    }
  }

  Future<void> fetchStatus() async {
    await _fetchStatus();
  }

  Future<void> toggleRelay(int index, bool value) async {
    if (_esp32IP == null || !_isConnected) {
      print('Cannot toggle relay: Not connected to ESP32');
      return;
    }

    // Optimistically update the UI
    final previousState = _relayStates[index];
    _relayStates[index] = value;
    notifyListeners();

    try {
      final response = await http
          .post(
            Uri.parse('http://$_esp32IP/toggle'),
            headers: {'Content-Type': 'application/json'},
            body: jsonEncode({
              'relay': (index + 1).toString(),
              'state': value ? '1' : '0',
            }),
          )
          .timeout(const Duration(seconds: 5));

      if (response.statusCode == 200) {
        final responseData = jsonDecode(response.body);
        if (responseData['success'] != true) {
          // Revert optimistic update on failure
          _relayStates[index] = previousState;
          notifyListeners();
          print('Failed to toggle relay: ${responseData['error']}');
        }
      } else {
        // Revert optimistic update on HTTP error
        _relayStates[index] = previousState;
        notifyListeners();
        print('Failed to toggle relay: HTTP ${response.statusCode}');
      }
    } catch (e) {
      // Revert optimistic update on exception
      _relayStates[index] = previousState;
      notifyListeners();
      print('Error toggling relay: $e');
    }
  }

  TimeOfDay _parseTime(String time) {
    final parts = time.split(':');
    return TimeOfDay(
      hour: int.parse(parts[0]),
      minute: int.parse(parts[1]),
    );
  }

  void _startPolling() {
    _pollingTimer?.cancel();
    _pollingTimer = Timer.periodic(const Duration(seconds: 1), (timer) {
      if (_isConnected) {
        _fetchStatus();
      } else {
        timer.cancel();
      }
    });
  }

  Future<void> disconnect() async {
    _pollingTimer?.cancel();
    try {
      if (_esp32IP != null) {
        await http
            .post(Uri.parse('http://$_esp32IP/disconnect'))
            .timeout(const Duration(seconds: 5));
      }
    } catch (e) {
      // Ignore errors during disconnect
    }
    _esp32IP = null;
    _isConnected = false;
    _ssid = null;
    final prefs = await SharedPreferences.getInstance();
    await prefs.remove('esp32IP');
    await prefs.remove('lastSSID');
    await prefs.remove('lastPassword');
    notifyListeners();
  }

  @override
  void dispose() {
    _pollingTimer?.cancel();
    super.dispose();
  }
}