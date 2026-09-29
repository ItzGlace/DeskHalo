using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Builder;
using System;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace DeskHalo_Host;
public static class FileTransfer
{
    public static string Root => Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments), "DeskHalo Transfers");
    public static string Outbox => Path.Combine(Root, "To Quest");
    public static string Inbox => Path.Combine(Root, "From Quest");
    public const long Limit = 2L * 1024 * 1024 * 1024;
    private static readonly SemaphoreSlim uploads = new(1);
    public static void Prepare() { Directory.CreateDirectory(Outbox); Directory.CreateDirectory(Inbox); }
    public static bool SafeName(string name) => name.Length is > 0 and <= 180 && name == Path.GetFileName(name) && !name.Contains(':') && !name.Contains('/') && !name.Contains('\\') && !name.EndsWith('.') && !name.EndsWith(' ') && name.IndexOfAny(Path.GetInvalidFileNameChars()) < 0 && !new[]{"CON","PRN","AUX","NUL","COM1","COM2","COM3","COM4","COM5","COM6","COM7","COM8","COM9","LPT1","LPT2","LPT3","LPT4","LPT5","LPT6","LPT7","LPT8","LPT9"}.Contains(name.Split('.')[0], StringComparer.OrdinalIgnoreCase);
    public static void Map(WebApplication app)
    {
        app.MapGet("/files", () => { Prepare(); return Results.Json(Directory.EnumerateFiles(Outbox).Select(p=>new FileInfo(p)).Where(f=>(f.Attributes & FileAttributes.ReparsePoint)==0 && SafeName(f.Name)).Select(f=>new {name=f.Name,size=f.Length})); });
        app.MapGet("/files/download", (HttpContext c) => {
            string name=c.Request.Query["name"].ToString(); if(!SafeName(name))return Results.BadRequest();
            string path=Path.Combine(Outbox,name); if(!File.Exists(path)||(File.GetAttributes(path)&FileAttributes.ReparsePoint)!=0)return Results.NotFound();
            return Results.File(path,"application/octet-stream",name,enableRangeProcessing:true);
        });
        app.MapPost("/files/upload", async (HttpContext c) => {
            string name=c.Request.Query["name"].ToString(); if(!SafeName(name))return Results.BadRequest("Invalid filename");
            if(c.Request.ContentLength>Limit)return Results.StatusCode(413);
            if(!await uploads.WaitAsync(0,c.RequestAborted))return Results.StatusCode(429);
            Prepare(); string temporary=Path.Combine(Inbox,".upload-"+Guid.NewGuid().ToString("N"));
            try {
                var feature=c.Features.Get<Microsoft.AspNetCore.Http.Features.IHttpMaxRequestBodySizeFeature>();if(feature is {IsReadOnly:false})feature.MaxRequestBodySize=Limit;
                await using(var output=new FileStream(temporary,FileMode.CreateNew,FileAccess.Write,FileShare.None,65536,true)) {
                    byte[] buffer=new byte[65536];long total=0;int n;
                    while((n=await c.Request.Body.ReadAsync(buffer,c.RequestAborted))>0){total+=n;if(total>Limit)return Results.StatusCode(413);await output.WriteAsync(buffer.AsMemory(0,n),c.RequestAborted);}
                }
                string destination=Path.Combine(Inbox,name); if(File.Exists(destination))destination=Path.Combine(Inbox,Guid.NewGuid().ToString("N")[..8]+"-"+name);
                File.Move(temporary,destination,false);return Results.Json(new {name=Path.GetFileName(destination)});
            } finally {if(File.Exists(temporary))File.Delete(temporary);uploads.Release();}
        });
    }
}
