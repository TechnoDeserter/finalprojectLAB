import 'dart:convert';
import 'package:flutter/material.dart';
import 'package:http/http.dart' as http;
import 'package:shared_preferences/shared_preferences.dart';

class ESP32Service with ChangeNotifier {
  String? _esp32IP;
  bool _isConnected = false;
  bool _isReconnecting = false;
  String? _ssid;

  String? get esp32IP => _esp32IP;
  bool get isConnected => _isConnected;
  bool get isReconnecting => _isReconnecting;
  String? get ssid => _ssid;

  ESP32Service() {
    _loadSavedIP();
  }

  Future<void> _loadSavedIP() async {
    final prefs = await SharedPreferences.getInstance();
    _esp32IP = prefs.getString('esp32IP');
    _ssid = prefs.getString('lastSSID');
    if (_esp32IP != null) {
      await tryReconnect();
    }
    notifyListeners();
  }

  Future<void> setESP32IP(String ip) async {
    _esp32IP = ip;
    _isConnected = true;
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString('esp32IP', ip);
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

  Future<void> disconnect() async {
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
}