package com.example.server.controller;

import com.baomidou.mybatisplus.core.conditions.query.QueryWrapper;
import com.example.server.entity.User;
import com.example.server.mapper.UserMapper;
import com.example.server.utils.JwtUtils;
import jakarta.validation.constraints.NotBlank;
import org.mindrot.jbcrypt.BCrypt;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.web.bind.annotation.*;

import java.util.HashMap;
import java.util.Map;

@RestController
@RequestMapping("/user")
@CrossOrigin(originPatterns = "*", allowCredentials = "true")
public class UserController {

    @Autowired(required = false)
    private UserMapper userMapper;

    @Autowired
    private JwtUtils jwtUtils;

    @PostMapping("/register")
    public Map<String, Object> register(@RequestBody User user) {
        Map<String, Object> result = new HashMap<>();
        try {
            if (user.getUsername() == null || user.getUsername().isBlank()) {
                result.put("code", 400);
                result.put("msg", "用户名不能为空");
                return result;
            }
            if (user.getPassword() == null || user.getPassword().length() < 6) {
                result.put("code", 400);
                result.put("msg", "密码至少6位");
                return result;
            }

            if (userMapper == null) {
                throw new RuntimeException("UserMapper 未注入");
            }

            QueryWrapper<User> query = new QueryWrapper<>();
            query.eq("username", user.getUsername());
            if (userMapper.selectCount(query) > 0) {
                result.put("code", 400);
                result.put("msg", "该账号已存在");
                return result;
            }

            if (user.getNickname() == null || user.getNickname().isBlank()) {
                user.setNickname("用户" + System.currentTimeMillis());
            }
            user.setRole("USER");

            // BCrypt 哈希密码
            user.setPassword(BCrypt.hashpw(user.getPassword(), BCrypt.gensalt()));

            userMapper.insert(user);

            result.put("code", 200);
            result.put("msg", "注册成功");
            user.setPassword(null); // 不返回密码
            result.put("data", user);
        } catch (Exception e) {
            result.put("code", 500);
            result.put("msg", "注册失败: " + e.getMessage());
        }
        return result;
    }

    @PostMapping("/login")
    public Map<String, Object> login(@RequestBody User loginUser) {
        Map<String, Object> result = new HashMap<>();
        try {
            if (loginUser.getUsername() == null || loginUser.getUsername().isBlank()) {
                result.put("code", 400);
                result.put("msg", "用户名不能为空");
                return result;
            }

            // 先按用户名查询
            QueryWrapper<User> query = new QueryWrapper<>();
            query.eq("username", loginUser.getUsername());
            User dbUser = userMapper.selectOne(query);

            if (dbUser == null) {
                result.put("code", 401);
                result.put("msg", "账号或密码错误");
                return result;
            }

            // BCrypt 验证密码
            if (!BCrypt.checkpw(loginUser.getPassword(), dbUser.getPassword())) {
                result.put("code", 401);
                result.put("msg", "账号或密码错误");
                return result;
            }

            // 生成 JWT Token
            String token = jwtUtils.generateToken(dbUser.getId(), dbUser.getUsername());

            result.put("code", 200);
            result.put("msg", "登录成功");
            result.put("token", token);
            dbUser.setPassword(null);
            result.put("userInfo", dbUser);
        } catch (Exception e) {
            result.put("code", 500);
            result.put("msg", "登录失败: " + e.getMessage());
        }
        return result;
    }
}
