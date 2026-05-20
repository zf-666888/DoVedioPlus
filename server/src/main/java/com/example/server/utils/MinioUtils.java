package com.example.server.utils;

import io.minio.GetPresignedObjectUrlArgs;
import io.minio.MinioClient;
import io.minio.PutObjectArgs;
import io.minio.RemoveObjectArgs;
import io.minio.http.Method;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Component;
import org.springframework.web.multipart.MultipartFile;

import java.io.InputStream;
import java.util.UUID;
import java.util.concurrent.TimeUnit;

@Component
public class MinioUtils {

    @Autowired
    private MinioClient minioClient;

    @Value("${minio.bucketName}")
    private String bucketName;

    @Value("${minio.endpoint}")
    private String endpoint;

    /**
     * 上传文件，返回对象名（不再直接返回公开 URL）
     */
    public String uploadFile(MultipartFile file) throws Exception {
        String originalFilename = file.getOriginalFilename();
        String suffix = "";
        if (originalFilename != null && originalFilename.contains(".")) {
            suffix = originalFilename.substring(originalFilename.lastIndexOf("."));
        }
        String newFilename = UUID.randomUUID().toString() + suffix;

        try (InputStream inputStream = file.getInputStream()) {
            minioClient.putObject(
                    PutObjectArgs.builder()
                            .bucket(bucketName)
                            .object(newFilename)
                            .stream(inputStream, file.getSize(), -1)
                            .contentType(file.getContentType())
                            .build()
            );
        }

        return endpoint + "/" + bucketName + "/" + newFilename;
    }

    /**
     * 生成预签名下载 URL（有效期 7 天）
     */
    public String getPresignedUrl(String objectName) {
        try {
            return minioClient.getPresignedObjectUrl(
                    GetPresignedObjectUrlArgs.builder()
                            .bucket(bucketName)
                            .object(objectName)
                            .method(Method.GET)
                            .expiry(7, TimeUnit.DAYS)
                            .build()
            );
        } catch (Exception e) {
            return endpoint + "/" + bucketName + "/" + objectName;
        }
    }

    /**
     * 从完整 URL 生成预签名下载链接
     */
    public String getPresignedUrlFromFileUrl(String fileUrl) {
        if (fileUrl == null || !fileUrl.startsWith("http")) return fileUrl;
        String objectName = fileUrl.substring(fileUrl.lastIndexOf("/") + 1);
        return getPresignedUrl(objectName);
    }

    /**
     * 从 MinIO 删除文件
     */
    public void removeFile(String fileUrl) {
        try {
            String objectName = fileUrl.substring(fileUrl.lastIndexOf("/") + 1);
            minioClient.removeObject(
                    RemoveObjectArgs.builder()
                            .bucket(bucketName)
                            .object(objectName)
                            .build()
            );
        } catch (Exception e) {
            System.err.println("MinIO 删除失败: " + e.getMessage());
        }
    }

    /**
     * 上传本地 File 对象到 MinIO
     */
    public String uploadLocalFile(java.io.File file) throws Exception {
        try (java.io.FileInputStream inputStream = new java.io.FileInputStream(file)) {
            minioClient.putObject(
                    PutObjectArgs.builder()
                            .bucket(bucketName)
                            .object(file.getName())
                            .stream(inputStream, file.length(), -1)
                            .contentType("video/mp4")
                            .build()
            );
        }
        return endpoint + "/" + bucketName + "/" + file.getName();
    }
}
