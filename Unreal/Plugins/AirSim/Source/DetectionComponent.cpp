// Fill out your copyright notice in the Description page of Project Settings.

#include "DetectionComponent.h"

#include <Components/SceneCaptureComponent2D.h>
#include <Components/StaticMeshComponent.h>
#include <DrawDebugHelpers.h>
#include <Engine/Engine.h>
#include <Engine/StaticMesh.h>
#include <Engine/TextureRenderTarget2D.h>
#include <EngineUtils.h>
#include <Math/UnrealMathUtility.h>
#include <Kismet/KismetSystemLibrary.h>
#include <Kismet/KismetMathLibrary.h>
#include <Engine/EngineTypes.h>

UDetectionComponent::UDetectionComponent()
    : max_distance_to_camera_(20000.f)
{
    // Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
    // off to improve performance if you don't need them.
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UDetectionComponent::BeginPlay()
{
    Super::BeginPlay();
    scene_capture_component_2D_ = CastChecked<USceneCaptureComponent2D>(GetAttachParent());
    object_filter_ = FObjectFilter();
}

void UDetectionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

const TArray<FDetectionInfo>& UDetectionComponent::getDetections()
{
    cached_detections_.Empty();

    for (TActorIterator<AActor> actor_itr(GetWorld()); actor_itr; ++actor_itr) {
        AActor* actor = *actor_itr;
        if (object_filter_.matchesActor(actor)) {
            if (FVector::Distance(actor->GetActorLocation(), GetComponentLocation()) <= max_distance_to_camera_) {
                FBox2D box_2D_out;
                if (texture_target_ && calcBoundingFromViewInfo(actor, box_2D_out)) {
                    FDetectionInfo detection;
                    detection.Actor = actor;
                    detection.Box2D = box_2D_out;

                    FBox box_3D = actor->GetComponentsBoundingBox(true);
                    detection.Box3D = FBox(getRelativeLocation(box_3D.Min), getRelativeLocation(box_3D.Max));

                    detection.RelativeTransform = FTransform(getRelativeRotation(actor->GetActorLocation(), actor->GetActorRotation()),
                                                             getRelativeLocation(actor->GetActorLocation()));
                    cached_detections_.Add(detection);
                }
            }
        }
    }

    return cached_detections_;
}

bool UDetectionComponent::calcBoundingFromViewInfo(AActor* actor, FBox2D& box_out)
{
    FVector origin;
    FVector extend;
    actor->GetActorBounds(false, origin, extend);

    TArray<FVector> points;
    TArray<FVector2D> points_2D;
    bool is_in_camera_view = false;

    // get render target for texture size
    FRenderTarget* render_target = texture_target_->GameThread_GetRenderTargetResource();

    // initialize viewinfo for projection matrix
    FMinimalViewInfo info;
    info.Location = scene_capture_component_2D_->GetComponentTransform().GetLocation();
    info.Rotation = scene_capture_component_2D_->GetComponentTransform().GetRotation().Rotator();
    info.FOV = scene_capture_component_2D_->FOVAngle;
    info.ProjectionMode = scene_capture_component_2D_->ProjectionType;
    info.AspectRatio = float(texture_target_->SizeX) / float(texture_target_->SizeY);
    info.OrthoNearClipPlane = 1;
    info.OrthoFarClipPlane = 100000;
    info.bConstrainAspectRatio = true;

    // calculate 3D corner Points of bounding box
    points.Add(origin + FVector(extend.X, extend.Y, extend.Z));
    points.Add(origin + FVector(-extend.X, extend.Y, extend.Z));
    points.Add(origin + FVector(extend.X, -extend.Y, extend.Z));
    points.Add(origin + FVector(-extend.X, -extend.Y, extend.Z));
    points.Add(origin + FVector(extend.X, extend.Y, -extend.Z));
    points.Add(origin + FVector(-extend.X, extend.Y, -extend.Z));
    points.Add(origin + FVector(extend.X, -extend.Y, -extend.Z));
    points.Add(origin + FVector(-extend.X, -extend.Y, -extend.Z));

    // initialize pixel values
    FVector2D min_pixel(texture_target_->SizeX, texture_target_->SizeY);
    FVector2D max_pixel(0, 0);
    FIntRect screen_rect(0, 0, texture_target_->SizeX, texture_target_->SizeY);

    // initialize projection data for sceneview
    FSceneViewProjectionData projection_data;
    projection_data.ViewOrigin = info.Location;

    // do some voodoo rotation that is somehow mandatory and stolen from UGameplayStatics::ProjectWorldToScreen
    projection_data.ViewRotationMatrix = FInverseRotationMatrix(info.Rotation) * FMatrix(
                                                                                     FPlane(0, 0, 1, 0),
                                                                                     FPlane(1, 0, 0, 0),
                                                                                     FPlane(0, 1, 0, 0),
                                                                                     FPlane(0, 0, 0, 1));

    if (scene_capture_component_2D_->bUseCustomProjectionMatrix) {
        projection_data.ProjectionMatrix = scene_capture_component_2D_->CustomProjectionMatrix;
    }
    else {
        projection_data.ProjectionMatrix = info.CalculateProjectionMatrix();
    }
    projection_data.SetConstrainedViewRectangle(screen_rect);

    // Project Points to pixels and get the corner pixels
    for (FVector& point : points) {
        FVector2D Pixel(0, 0);
        FSceneView::ProjectWorldToScreen((point), screen_rect, projection_data.ComputeViewProjectionMatrix(), Pixel);
        is_in_camera_view |= (Pixel != screen_rect.Min) && (Pixel != screen_rect.Max) && screen_rect.Contains(FIntPoint(Pixel.X, Pixel.Y));
        points_2D.Add(Pixel);
        max_pixel.X = FMath::Max(Pixel.X, max_pixel.X);
        max_pixel.Y = FMath::Max(Pixel.Y, max_pixel.Y);
        min_pixel.X = FMath::Min(Pixel.X, min_pixel.X);
        min_pixel.Y = FMath::Min(Pixel.Y, min_pixel.Y);
    }

    // If actor in camera view - check if it's actually visible or hidden
    // Check against 8 extend points
    bool is_visible = false;
    if (is_in_camera_view) {
        FHitResult result;
        bool is_world_hit;
        for (FVector& point : points) {
            is_world_hit = GetWorld()->LineTraceSingleByChannel(result, GetComponentLocation(), point, ECC_WorldStatic);
            if (is_world_hit) {
                if (result.Actor == actor) {
                    is_visible = true;
                    break;
                }
            }
        }

        // If actor in camera view but didn't hit any point out of 8 extend points,
        // check against 10 random points
        if (!is_visible) {
            for (int i = 0; i < 10; i++) {
                FVector point = UKismetMathLibrary::RandomPointInBoundingBox(origin, extend);
                is_world_hit = GetWorld()->LineTraceSingleByChannel(result, GetComponentLocation(), point, ECC_WorldStatic);
                if (is_world_hit) {
                    if (result.Actor == actor) {
                        is_visible = true;
                        break;
                    }
                }
            }
        }
    }

    FBox2D box_out_temp = FBox2D(min_pixel, max_pixel);

    box_out.Min.X = FMath::Clamp<float>(box_out_temp.Min.X, 0, texture_target_->SizeX);
    box_out.Min.Y = FMath::Clamp<float>(box_out_temp.Min.Y, 0, texture_target_->SizeY);
    box_out.Max.X = FMath::Clamp<float>(box_out_temp.Max.X, 0, texture_target_->SizeX);
    box_out.Max.Y = FMath::Clamp<float>(box_out_temp.Max.Y, 0, texture_target_->SizeY);

    return is_in_camera_view && is_visible;
}

FVector UDetectionComponent::getRelativeLocation(FVector in_location)
{
    return GetComponentTransform().InverseTransformPosition(in_location);
}

FRotator UDetectionComponent::getRelativeRotation(FVector in_location, FRotator in_rotation)
{
    FTransform camera_transform(GetComponentRotation(), GetComponentLocation());
    FTransform relative_object_transform = camera_transform.GetRelativeTransform(FTransform(in_rotation, in_location));
    return relative_object_transform.Rotator();
}

void UDetectionComponent::addMeshName(const std::string& mesh_name)
{
    FString name(mesh_name.c_str());

    if (!object_filter_.wildcard_mesh_names_.Contains(name)) {
        object_filter_.wildcard_mesh_names_.Add(name);
    }
}

void UDetectionComponent::setFilterRadius(const float radius_cm)
{
    max_distance_to_camera_ = radius_cm;
}

void UDetectionComponent::clearMeshNames()
{
    object_filter_.wildcard_mesh_names_.Empty();
}
